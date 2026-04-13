#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
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
    char     lastMsgType[20];
    char     lastError[24];
};
static DisplayState      displayState = {0, "none", "none"};
static SemaphoreHandle_t displayMutex;

// ─────────────────────────────────────────────────────────────────────────────
// Task: Receiver  (priority 3 — highest)
//
// Blocks on SharedBus::poll() which blocks on _rxSemaphore.
// When the ISR gives the semaphore, this task copies the buffer and puts it
// on inboundQueue. No JSON, no Serial, no OLED here — keep this path fast.
// ─────────────────────────────────────────────────────────────────────────────
void receiverTask(void* params) {
    char buf[256];

    while (true) {
        if (sharedBus.poll(buf, sizeof(buf))) {
            if (xQueueSend(inboundQueue, buf, portMAX_DELAY) != pdTRUE) {
                Serial.println("[MCU2-RX] queue full — message dropped");
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task: Logic  (priority 2)
//
// Drains inboundQueue. Parses, validates, and handles each message.
// For HEARTBEAT: sends HEARTBEAT_ACK back to MCU #1.
//
// This is where assumption A3 is verified — any garbled message from
// overlapping sends will fail MessageProtocol::validate() and appear here
// as an error. A clean run means zero validation failures.
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
            // A3 failure signal — garbled JSON arrived.
            Serial.printf("[MCU2-LG] INVALID message: %s\n", res.detail);

            xSemaphoreTake(displayMutex, portMAX_DELAY);
            snprintf(displayState.lastError, sizeof(displayState.lastError),
                "%s", res.detail);
            xSemaphoreGive(displayMutex);
            continue;
        }

        uint8_t type = doc[Field::TYPE];
        uint8_t from = doc[Field::FROM];
        rxCount++;

        Serial.printf("[MCU2-LG] received %s from 0x%02X (#%lu)\n",
            MessageProtocol::msgTypeName(type), from, rxCount);

        xSemaphoreTake(displayMutex, portMAX_DELAY);
        displayState.rxCount = rxCount;
        strncpy(displayState.lastMsgType,
            MessageProtocol::msgTypeName(type),
            sizeof(displayState.lastMsgType) - 1);
        xSemaphoreGive(displayMutex);

        // Handle by type.
        switch (type) {
            case MsgType::HEARTBEAT: {
                // Build and send ACK back to sender.
                JsonDocument ack = MessageProtocol::createEnvelope(
                    I2C_ADDRESS, from, MsgType::HEARTBEAT_ACK
                );
                ack[Field::PAYLOAD][Field::STATUS] = Status::OK;
                ack[Field::PAYLOAD][Field::UPTIME] = millis();

                if (!MessageProtocol::serialize(ack, txBuf, sizeof(txBuf))) {
                    Serial.println("[MCU2-LG] ACK serialize failed");
                    break;
                }

                BusError err = sharedBus.send(from, txBuf);
                Serial.printf("[MCU2-LG] ACK → 0x%02X: %s\n",
                    from,
                    err == BusError::OK ? "OK" : "FAILED");
                break;
            }

            default:
                Serial.printf("[MCU2-LG] unhandled type: %d\n", type);
                break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task: OLED  (priority 1 — lowest)
//
// Reads a snapshot of displayState under displayMutex, updates OLED every
// 500ms. Low priority means bus and logic tasks are never delayed by display.
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

        oled.showStatus("TRANS PROCESSOR", line2, line3, line4);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("[MCU2] Transaction Processor starting — FreeRTOS PoC");

    if (!oled.begin()) {
        Serial.println("[MCU2] OLED failed");
    }
    oled.showStatus("TRANS PROCESSOR", "Starting...", "", "");

    // init() creates busMutex and rxSemaphore, then calls beginSlave().
    // Must happen before xTaskCreate().
    sharedBus.init(I2C_ADDRESS);

    displayMutex = xSemaphoreCreateMutex();
    if (displayMutex == nullptr) {
        Serial.println("[MCU2] FATAL: displayMutex failed");
        return;
    }

    inboundQueue = xQueueCreate(8, sizeof(char[256]));
    if (inboundQueue == nullptr) {
        Serial.println("[MCU2] FATAL: inboundQueue failed");
        return;
    }

    xTaskCreate(receiverTask, "Receiver", STACK_SIZE_RECEIVER, NULL, 3, NULL);
    xTaskCreate(logicTask,    "Logic",    STACK_SIZE_LOGIC,    NULL, 2, NULL);
    xTaskCreate(oledTask,     "OLED",     STACK_SIZE_OLED,     NULL, 1, NULL);

    Serial.println("[MCU2] Tasks created — scheduler already running");}

void loop() {
    // FreeRTOS scheduler runs setup() and loop() inside loopTask().
    // All work is done in tasks created in setup(). loop() is never used.
    vTaskDelete(NULL); // optional: delete loopTask to reclaim its stack
}