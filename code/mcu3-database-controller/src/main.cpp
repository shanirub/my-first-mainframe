#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "config.h"
#include "oled_display.h"
#include "shared_bus.h"
#include "message_protocol.h"

// ─────────────────────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────────────────────
OledDisplay oled(OLED_SDA_PIN, OLED_SCL_PIN);
SharedBus   sharedBus;

// Inbound queue: receiver task → logic task.
QueueHandle_t inboundQueue;

// Display state — written by logic task, read by OLED task.
struct DisplayState {
    uint32_t rxCount;
    char     lastMsgType[16];
    char     lastError[24];
};
static DisplayState      displayState = {0, "none", "none"};
static SemaphoreHandle_t displayMutex;

// Static: must outlive setup(). Do NOT move inside setup() — task holds a pointer to this.
static ReceiverParams receiverParams;

// ─────────────────────────────────────────────────────────────────────────────
// Task: Logic  (priority 2, STACK_SIZE_LOGIC)
//
// Drains inboundQueue. Parses and validates each message.
// For HEARTBEAT: sends HEARTBEAT_ACK back to the sender.
// ─────────────────────────────────────────────────────────────────────────────
void logicTask(void* params) {
    char     buf[256];
    char     txBuf[256];
    uint32_t rxCount = 0;

    while (true) {
        if (xQueueReceive(inboundQueue, buf, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        JsonDocument     doc = MessageProtocol::parse(buf);
        ValidationResult res = MessageProtocol::validate(doc);

        if (!res.valid) {
            Serial.printf("[MCU3-LG] INVALID message: %s\n", res.detail);

            xSemaphoreTake(displayMutex, portMAX_DELAY);
            snprintf(displayState.lastError, sizeof(displayState.lastError),
                "%s", res.detail);
            xSemaphoreGive(displayMutex);
            continue;
        }

        uint8_t type = doc[Field::TYPE];
        uint8_t from = doc[Field::FROM];
        rxCount++;

        xSemaphoreTake(displayMutex, portMAX_DELAY);
        displayState.rxCount = rxCount;
        strncpy(displayState.lastMsgType,
            MessageProtocol::msgTypeName(type),
            sizeof(displayState.lastMsgType) - 1);
        xSemaphoreGive(displayMutex);

        switch (type) {
            case MsgType::HEARTBEAT: {
                Serial.printf("[MCU3-LG] received HEARTBEAT from 0x%02X (#%lu)\n",
                    from, rxCount);

                JsonDocument ack = MessageProtocol::createEnvelope(
                    I2C_ADDRESS, from, MsgType::HEARTBEAT_ACK
                );
                ack[Field::PAYLOAD][Field::STATUS] = Status::OK;
                ack[Field::PAYLOAD][Field::UPTIME] = millis();

                if (!MessageProtocol::serialize(ack, txBuf, sizeof(txBuf))) {
                    Serial.println("[MCU3-LG] ACK serialize failed");
                    break;
                }

                BusError err = sharedBus.send(from, txBuf);
                Serial.printf("[MCU3-LG] ACK → 0x%02X: %s\n",
                    from, err == BusError::OK ? "OK" : "FAILED");
                break;
            }

            default:
                Serial.printf("[MCU3-LG] unhandled type: %d\n", type);
                break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task: OLED  (priority 1 — lowest)
// ─────────────────────────────────────────────────────────────────────────────
void oledTask(void* params) {
    char line2[20], line3[20], line4[20];

    while (true) {
        DisplayState snap;
        xSemaphoreTake(displayMutex, portMAX_DELAY);
        snap = displayState;
        xSemaphoreGive(displayMutex);

        snprintf(line2, sizeof(line2), "Addr: 0x%02X", I2C_ADDRESS);
        snprintf(line3, sizeof(line3), "RX: %lu", snap.rxCount);
        snprintf(line4, sizeof(line4), "Last: %s", snap.lastMsgType);

        oled.showStatus("DATABASE CTRL", line2, line3, line4);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("[MCU3] Database Controller starting — Phase 3");

    if (!oled.begin()) {
        Serial.println("[MCU3] OLED failed");
    }
    oled.showStatus("DATABASE CTRL", "Starting...", "", "");

    sharedBus.init(I2C_ADDRESS);

    displayMutex = xSemaphoreCreateMutex();
    if (displayMutex == nullptr) {
        Serial.println("[MCU3] FATAL: displayMutex failed");
        return;
    }

    inboundQueue = xQueueCreate(8, sizeof(char[256]));
    if (inboundQueue == nullptr) {
        Serial.println("[MCU3] FATAL: inboundQueue failed");
        return;
    }

    receiverParams = { &sharedBus, inboundQueue };

    xTaskCreate(receiverTask, "Receiver", STACK_SIZE_RECEIVER, &receiverParams, 3, NULL);
    xTaskCreate(logicTask,    "Logic",    STACK_SIZE_LOGIC,    NULL,            2, NULL);
    xTaskCreate(oledTask,     "OLED",     STACK_SIZE_OLED,     NULL,            1, NULL);

    Serial.println("[MCU3] Tasks created — scheduler already running");
}

void loop() {
    vTaskDelete(NULL);
}
