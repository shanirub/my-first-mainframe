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
// Depth 8: at most a few ACKs can arrive before logic task drains them.
QueueHandle_t inboundQueue;

// Display state — written by logic and sender tasks, read by OLED task.
// Protected by displayMutex to prevent the OLED task reading a half-updated
// struct during a context switch.
struct DisplayState {
    uint32_t sendCountA;
    uint32_t sendCountB;
    uint32_t ackCount;
    char     lastError[24];
};
static DisplayState    displayState = {0, 0, 0, "none"};
static SemaphoreHandle_t displayMutex;

// ─────────────────────────────────────────────────────────────────────────────
// Task: Sender A  (priority 2, runs every 2 seconds)
//
// Tests assumption A1 (mode switching) and A3 (mutex) together with Sender B.
// Deliberately uses a different interval from Sender B so they occasionally
// collide and both try to acquire busMutex simultaneously.
// ─────────────────────────────────────────────────────────────────────────────
void senderTaskA(void* params) {
    uint32_t count = 0;
    char buf[256];

    while (true) {
        count++;

        JsonDocument doc = MessageProtocol::createEnvelope(
            I2C_ADDRESS, ADDR_TRANSACTION_PROCESSOR, MsgType::HEARTBEAT
        );
        doc[Field::PAYLOAD][Field::TIMESTAMP] = millis();

        if (!MessageProtocol::serialize(doc, buf, sizeof(buf))) {
            Serial.printf("[MCU1-A] serialize failed (#%lu)\n", count);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        BusError err = sharedBus.send(ADDR_TRANSACTION_PROCESSOR, buf);

        Serial.printf("[MCU1-A] HEARTBEAT → 0x%02X: %s (#%lu)\n",
            ADDR_TRANSACTION_PROCESSOR,
            err == BusError::OK ? "OK" : "FAILED",
            count);

        if (err == BusError::OK) {
            xSemaphoreTake(displayMutex, portMAX_DELAY);
            displayState.sendCountA = count;
            xSemaphoreGive(displayMutex);
        } else {
            xSemaphoreTake(displayMutex, portMAX_DELAY);
            snprintf(displayState.lastError, sizeof(displayState.lastError),
                "A err %d", (int)err);
            xSemaphoreGive(displayMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task: Sender B  (priority 2, runs every 3 seconds)
//
// Same as Sender A but at a different interval. The 2s/3s combination means
// they collide every 6 seconds — one will block on busMutex while the other
// transmits. This is the mutex stress test.
// ─────────────────────────────────────────────────────────────────────────────
void senderTaskB(void* params) {
    uint32_t count = 0;
    char buf[256];

    while (true) {
        count++;

        JsonDocument doc = MessageProtocol::createEnvelope(
            I2C_ADDRESS, ADDR_TRANSACTION_PROCESSOR, MsgType::HEARTBEAT
        );
        doc[Field::PAYLOAD][Field::TIMESTAMP] = millis();

        if (!MessageProtocol::serialize(doc, buf, sizeof(buf))) {
            Serial.printf("[MCU1-B] serialize failed (#%lu)\n", count);
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        BusError err = sharedBus.send(ADDR_TRANSACTION_PROCESSOR, buf);

        Serial.printf("[MCU1-B] HEARTBEAT → 0x%02X: %s (#%lu)\n",
            ADDR_TRANSACTION_PROCESSOR,
            err == BusError::OK ? "OK" : "FAILED",
            count);

        if (err == BusError::OK) {
            xSemaphoreTake(displayMutex, portMAX_DELAY);
            displayState.sendCountB = count;
            xSemaphoreGive(displayMutex);
        } else {
            xSemaphoreTake(displayMutex, portMAX_DELAY);
            snprintf(displayState.lastError, sizeof(displayState.lastError),
                "B err %d", (int)err);
            xSemaphoreGive(displayMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task: Receiver  (priority 3 — highest, must wake immediately on ISR signal)
//
// Blocks on SharedBus::poll() which internally blocks on _rxSemaphore.
// When the ISR gives the semaphore (bytes arrived), this task wakes,
// copies the buffer, and puts it on inboundQueue for the logic task.
// Does no JSON, no Serial, no OLED — minimal work in this hot path.
// ─────────────────────────────────────────────────────────────────────────────
void receiverTask(void* params) {
    char buf[256];

    while (true) {
        if (sharedBus.poll(buf, sizeof(buf))) {
            // Put on queue — logic task drains it.
            // portMAX_DELAY: if queue is full, block until space available.
            if (xQueueSend(inboundQueue, buf, portMAX_DELAY) != pdTRUE) {
                Serial.println("[MCU1-RX] queue full — message dropped");
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task: Logic  (priority 2)
//
// Drains inboundQueue. Parses and validates each message.
// Prints result to serial and updates display state.
// This is where assumption A2 (queue integrity) is verified —
// every ACK sent by MCU #2 should appear here.
// ─────────────────────────────────────────────────────────────────────────────
void logicTask(void* params) {
    char    buf[256];
    uint32_t ackCount = 0;

    while (true) {
        // Block until receiver task puts something on the queue.
        if (xQueueReceive(inboundQueue, buf, portMAX_DELAY) == pdTRUE) {
            JsonDocument     doc = MessageProtocol::parse(buf);
            ValidationResult res = MessageProtocol::validate(doc);

            if (!res.valid) {
                Serial.printf("[MCU1-LG] invalid message: %s\n", res.detail);
                continue;
            }

            uint8_t type = doc[Field::TYPE];
            uint8_t from = doc[Field::FROM];
            ackCount++;

            Serial.printf("[MCU1-LG] received %s from 0x%02X (#%lu)\n",
                MessageProtocol::msgTypeName(type), from, ackCount);

            xSemaphoreTake(displayMutex, portMAX_DELAY);
            displayState.ackCount = ackCount;
            xSemaphoreGive(displayMutex);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task: OLED  (priority 1 — lowest, display lag is invisible)
//
// Reads a snapshot of displayState under displayMutex, then updates the OLED.
// Runs every 500ms. The mutex ensures it never reads a half-updated struct.
// ─────────────────────────────────────────────────────────────────────────────
void oledTask(void* params) {
    char line2[20], line3[20], line4[20];

    while (true) {
        DisplayState snap;
        xSemaphoreTake(displayMutex, portMAX_DELAY);
        snap = displayState;
        xSemaphoreGive(displayMutex);

        snprintf(line2, sizeof(line2), "A:%lu B:%lu",
            snap.sendCountA, snap.sendCountB);
        snprintf(line3, sizeof(line3), "ACKs: %lu", snap.ackCount);
        snprintf(line4, sizeof(line4), "Err: %s", snap.lastError);

        oled.showStatus("MASTER CONSOLE", line2, line3, line4);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("[MCU1] Master Console starting — FreeRTOS PoC");

    if (!oled.begin()) {
        Serial.println("[MCU1] OLED failed");
    }
    oled.showStatus("MASTER CONSOLE", "Starting...", "", "");

    // init() creates busMutex and rxSemaphore, then calls beginSlave().
    // Must happen before xTaskCreate() so semaphores exist before ISR fires.
    sharedBus.init(I2C_ADDRESS);

    // Display mutex: separate from busMutex — protects displayState only.
    displayMutex = xSemaphoreCreateMutex();
    if (displayMutex == nullptr) {
        Serial.println("[MCU1] FATAL: displayMutex failed");
        return;
    }

    // Inbound queue: 8 slots of 256 bytes each.
    inboundQueue = xQueueCreate(8, sizeof(char[256]));
    if (inboundQueue == nullptr) {
        Serial.println("[MCU1] FATAL: inboundQueue failed");
        return;
    }

    // Create tasks. NULL = no task handle needed for this PoC.
    xTaskCreate(senderTaskA, "SenderA",  STACK_SIZE_SENDER,   NULL, 2, NULL);
    xTaskCreate(senderTaskB, "SenderB",  STACK_SIZE_SENDER,   NULL, 2, NULL);
    xTaskCreate(receiverTask, "Receiver", STACK_SIZE_RECEIVER, NULL, 3, NULL);
    xTaskCreate(logicTask,   "Logic",    STACK_SIZE_LOGIC,    NULL, 2, NULL);
    xTaskCreate(oledTask,    "OLED",     STACK_SIZE_OLED,     NULL, 1, NULL);
    
    Serial.println("[MCU1] Tasks created — scheduler already running");
}

void loop() {
    // FreeRTOS scheduler runs setup() and loop() inside loopTask().
    // All work is done in tasks created in setup(). loop() is never used.
    vTaskDelete(NULL); // optional: delete loopTask to reclaim its stack
}