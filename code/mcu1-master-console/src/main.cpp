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

// inboundQueue: receiver task → logic task  (8 slots × 256 bytes)
QueueHandle_t inboundQueue;

// serialQueue: serialInput task → logic task  (4 slots × 64 bytes)
QueueHandle_t serialQueue;

// Shared state — written by heartbeat + logic tasks, read by OLED task.
// Protected by displayMutex to prevent the OLED task reading a half-updated
// struct during a context switch.
struct SharedState {
    uint32_t lastAckTime[4];  // indexed by slave order: 0=0x09, 1=0x0A, 2=0x0B, 3=0x0C
    uint32_t heartbeatCount;
    char     lastError[24];
};
static SharedState       sharedState = {{0, 0, 0, 0}, 0, "none"};
static SemaphoreHandle_t displayMutex;

// Static: must outlive setup(). Do NOT move inside setup() — task holds a pointer to this.
static ReceiverParams receiverParams;

// ─────────────────────────────────────────────────────────────────────────────
// Task: Heartbeat  (priority 2, STACK_SIZE_SENDER)
//
// Sends HEARTBEAT sequentially to all 4 slave MCUs every HEARTBEAT_INTERVAL_MS.
// ─────────────────────────────────────────────────────────────────────────────
void heartbeatTask(void* params) {
    uint32_t count = 0;
    char     buf[256];

    const uint8_t targets[] = {
        ADDR_TRANSACTION_PROCESSOR,
        ADDR_DATABASE_CONTROLLER,
        ADDR_JOB_SCHEDULER,
        ADDR_IO_CONTROLLER
    };

    while (true) {
        count++;

        for (uint8_t i = 0; i < 4; i++) {
            uint8_t target = targets[i];

            JsonDocument doc = MessageProtocol::createEnvelope(
                I2C_ADDRESS, target, MsgType::HEARTBEAT
            );
            doc[Field::PAYLOAD][Field::TIMESTAMP] = millis();

            if (!MessageProtocol::serialize(doc, buf, sizeof(buf))) {
                Serial.printf("[MCU1-HB] HEARTBEAT → 0x%02X: serialize failed (#%lu)\n",
                    target, count);
                continue;
            }

            BusError err = sharedBus.send(target, buf);
            Serial.printf("[MCU1-HB] HEARTBEAT → 0x%02X: %s (#%lu)\n",
                target,
                err == BusError::OK ? "OK" : "FAILED",
                count);

            // Give the slave 500ms to process and ACK before we switch to
            // master again for the next send. Without this, MCU #1 re-enters
            // master mode before the ACK arrives and the slave sees address
            // 0x08 as absent on the bus.
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        xSemaphoreTake(displayMutex, portMAX_DELAY);
        sharedState.heartbeatCount = count;
        xSemaphoreGive(displayMutex);

        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task: Serial Input  (priority 2, STACK_SIZE_LOGIC)
//
// Reads newline-terminated commands from USB serial monitor.
// STATUS is handled immediately (reads sharedState under mutex).
// All other valid commands are forwarded to serialQueue for logicTask.
// ─────────────────────────────────────────────────────────────────────────────
void serialInputTask(void* params) {
    while (true) {
        if (!Serial.available()) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        String line = Serial.readStringUntil('\n');
        line.trim();

        if (line.length() == 0) continue;

        if (line.startsWith("STATUS")) {
            xSemaphoreTake(displayMutex, portMAX_DELAY);
            SharedState snap = sharedState;
            xSemaphoreGive(displayMutex);

            uint32_t now    = millis();
            uint8_t  active = 0;
            for (int i = 0; i < 4; i++) {
                if (snap.lastAckTime[i] > 0 &&
                    (now - snap.lastAckTime[i]) < HEARTBEAT_ACK_TIMEOUT_MS) {
                    active++;
                }
            }
            Serial.printf("[MCU1-SER] STATUS: activeSubsystems=%d/4, heartbeatCount=%lu\n",
                active, snap.heartbeatCount);
            continue;
        }

        // Validate known commands before queuing.
        bool known = line.startsWith("DEPOSIT ")
                  || line.startsWith("WITHDRAW ")
                  || line.startsWith("BALANCE ");

        if (!known) {
            Serial.printf("[MCU1-SER] unknown command: %s\n", line.c_str());
            continue;
        }

        // Put raw trimmed command string on serialQueue (64-byte slot).
        char cmdBuf[64];
        strncpy(cmdBuf, line.c_str(), sizeof(cmdBuf) - 1);
        cmdBuf[sizeof(cmdBuf) - 1] = '\0';

        if (xQueueSend(serialQueue, cmdBuf, 0) != pdTRUE) {
            Serial.println("[MCU1-SER] serialQueue full — command dropped");
        }
    }
}

// Returns the lastAckTime index for a given slave address.
// Returns -1 if address is not a known slave.
static int8_t slaveIndex(uint8_t addr) {
    switch (addr) {
        case ADDR_TRANSACTION_PROCESSOR: return 0;
        case ADDR_DATABASE_CONTROLLER:   return 1;
        case ADDR_JOB_SCHEDULER:         return 2;
        case ADDR_IO_CONTROLLER:         return 3;
        default:                         return -1;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task: Logic  (priority 2, STACK_SIZE_LOGIC)
//
// Drains inboundQueue (blocking) and serialQueue (non-blocking poll after each
// inbound message). Handles ACKs, errors, and serial commands.
// ─────────────────────────────────────────────────────────────────────────────
void logicTask(void* params) {
    char     buf[256];
    char     cmdBuf[64];
    uint32_t ackCount = 0;

    while (true) {
        // Block until receiver task puts something on inboundQueue.
        if (xQueueReceive(inboundQueue, buf, portMAX_DELAY) == pdTRUE) {
            JsonDocument     doc = MessageProtocol::parse(buf);
            ValidationResult res = MessageProtocol::validate(doc);

            if (!res.valid) {
                Serial.printf("[MCU1-LG] invalid message: %s\n", res.detail);

                xSemaphoreTake(displayMutex, portMAX_DELAY);
                snprintf(sharedState.lastError, sizeof(sharedState.lastError),
                    "%s", res.detail);
                xSemaphoreGive(displayMutex);
            } else {
                uint8_t type = doc[Field::TYPE];
                uint8_t from = doc[Field::FROM];

                switch (type) {
                    case MsgType::HEARTBEAT_ACK: {
                        ackCount++;
                        Serial.printf("[MCU1-LG] received HEARTBEAT_ACK from 0x%02X (#%lu)\n",
                            from, ackCount);

                        int8_t idx = slaveIndex(from);
                        if (idx >= 0) {
                            xSemaphoreTake(displayMutex, portMAX_DELAY);
                            sharedState.lastAckTime[idx] = millis();
                            xSemaphoreGive(displayMutex);
                        }
                        break;
                    }

                    case MsgType::ERROR: {
                        Serial.printf("[MCU1-LG] ERROR from 0x%02X\n", from);

                        xSemaphoreTake(displayMutex, portMAX_DELAY);
                        snprintf(sharedState.lastError, sizeof(sharedState.lastError),
                            "ERR from 0x%02X", from);
                        xSemaphoreGive(displayMutex);
                        break;
                    }

                    default:
                        Serial.printf("[MCU1-LG] unhandled type: %d from 0x%02X\n",
                            type, from);
                        break;
                }
            }
        }

        // Drain any pending serial commands (non-blocking).
        while (xQueueReceive(serialQueue, cmdBuf, 0) == pdTRUE) {
            // Phase 3 stub — actual dispatch to MCU #4 comes in Phase 4.
            Serial.printf("[MCU1-LG] serial cmd (stub): %s\n", cmdBuf);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task: OLED  (priority 1 — lowest)
//
// Snapshots sharedState under displayMutex every 500ms, then updates display.
// ─────────────────────────────────────────────────────────────────────────────
void oledTask(void* params) {
    char line2[20], line3[20];

    while (true) {
        SharedState snap;
        xSemaphoreTake(displayMutex, portMAX_DELAY);
        snap = sharedState;
        xSemaphoreGive(displayMutex);

        // Compute active subsystem count from last ACK timestamps.
        // A slave is active if it ACKed within HEARTBEAT_ACK_TIMEOUT_MS.
        uint32_t now    = millis();
        uint8_t  active = 0;
        for (int i = 0; i < 4; i++) {
            if (snap.lastAckTime[i] > 0 &&
                (now - snap.lastAckTime[i]) < HEARTBEAT_ACK_TIMEOUT_MS) {
                active++;
            }
        }

        snprintf(line2, sizeof(line2), "HB:%lu Act:%d/4",
            snap.heartbeatCount, active);
        snprintf(line3, sizeof(line3), "Err:%s", snap.lastError);

        oled.showStatus("MASTER CONSOLE", line2, line3, "");

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("[MCU1] Master Console starting — Phase 3");

    if (!oled.begin()) {
        Serial.println("[MCU1] OLED failed");
    }
    oled.showStatus("MASTER CONSOLE", "Starting...", "", "");

    // init() creates busMutex and rxSemaphore, then calls _switchToSlave().
    // Must happen before xTaskCreate() so semaphores exist before ISR fires.
    sharedBus.init(I2C_ADDRESS);

    displayMutex = xSemaphoreCreateMutex();
    if (displayMutex == nullptr) {
        Serial.println("[MCU1] FATAL: displayMutex failed");
        return;
    }

    inboundQueue = xQueueCreate(8, sizeof(char[256]));
    if (inboundQueue == nullptr) {
        Serial.println("[MCU1] FATAL: inboundQueue failed");
        return;
    }

    serialQueue = xQueueCreate(4, sizeof(char[64]));
    if (serialQueue == nullptr) {
        Serial.println("[MCU1] FATAL: serialQueue failed");
        return;
    }

    receiverParams = { &sharedBus, inboundQueue };

    xTaskCreate(receiverTask,    "Receiver",  STACK_SIZE_RECEIVER, &receiverParams, 3, NULL);
    xTaskCreate(heartbeatTask,   "Heartbeat", STACK_SIZE_SENDER,   NULL,            2, NULL);
    xTaskCreate(serialInputTask, "SerialIn",  STACK_SIZE_LOGIC,    NULL,            2, NULL);
    xTaskCreate(logicTask,       "Logic",     STACK_SIZE_LOGIC,    NULL,            2, NULL);
    xTaskCreate(oledTask,        "OLED",      STACK_SIZE_OLED,     NULL,            1, NULL);

    Serial.println("[MCU1] Tasks created — scheduler already running");
}

void loop() {
    vTaskDelete(NULL);
}
