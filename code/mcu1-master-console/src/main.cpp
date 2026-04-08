#include <Arduino.h>
#include "config.h"
#include "oled_display.h"
#include "shared_bus.h"
#include "message_protocol.h"

OledDisplay oled(OLED_SDA_PIN, OLED_SCL_PIN);
SharedBus   sharedBus;

// Slave addresses to poll
const uint8_t SLAVES[]     = { ADDR_TRANSACTION_PROCESSOR,
                                ADDR_DATABASE_CONTROLLER,
                                ADDR_JOB_SCHEDULER,
                                ADDR_IO_CONTROLLER };
const uint8_t SLAVE_COUNT  = 4;

uint8_t  respondingCount   = 0;
uint32_t lastHeartbeat     = 0;
char     rxBuf[256];

void sendHeartbeats() {
    Serial.println("[MCU1] --- sending heartbeats ---");
    respondingCount = 0;

    for (uint8_t i = 0; i < SLAVE_COUNT; i++) {
        uint8_t target = SLAVES[i];

        JsonDocument doc = MessageProtocol::createEnvelope(
            I2C_ADDRESS, target, MsgType::HEARTBEAT
        );
        doc[Field::PAYLOAD][Field::TIMESTAMP] = millis();

        char buf[256];
        if (!MessageProtocol::serialize(doc, buf, sizeof(buf))) continue;

        BusError err = sharedBus.send(target, buf);
        if (err == BusError::OK) {
            Serial.printf("[MCU1] HEARTBEAT → 0x%02X: OK\n", target);
            respondingCount++;
        } else {
            Serial.printf("[MCU1] HEARTBEAT → 0x%02X: FAILED (%d)\n", target, (int)err);
        }
        delay(50); // small gap between transmissions
    }

    Serial.printf("[MCU1] %d/%d slaves responding\n", respondingCount, SLAVE_COUNT);
}

void updateOled() {
    char line2[20], line3[20];
    snprintf(line2, sizeof(line2), "Slaves: %d/%d", respondingCount, SLAVE_COUNT);
    snprintf(line3, sizeof(line3), "Last: %lums", millis() / 1000);
    oled.showStatus("MASTER CONSOLE", "HEARTBEAT TEST", line2, line3);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("[MCU1] Master Console starting...");

    if (!oled.begin()) {
        Serial.println("[MCU1] OLED failed");
    }
    oled.showStatus("MASTER CONSOLE", "Starting...", "", "");

    sharedBus.beginMaster();
    Serial.println("[MCU1] SharedBus master ready");
}

void loop() {
    // Send heartbeats every 3 seconds
    if (millis() - lastHeartbeat > 3000) {
        lastHeartbeat = millis();
        sendHeartbeats();
        updateOled();
    }

    // Drain any incoming messages (ACKs from slaves)
    if (sharedBus.poll(rxBuf, sizeof(rxBuf))) {
        JsonDocument doc     = MessageProtocol::parse(rxBuf);
        ValidationResult res = MessageProtocol::validate(doc);

        if (!res.valid) {
            Serial.printf("[MCU1] Invalid message: %s\n", res.detail);
            return;
        }

        uint8_t type = doc[Field::TYPE];
        uint8_t from = doc[Field::FROM];

        Serial.printf("[MCU1] Received %s from 0x%02X\n",
            MessageProtocol::msgTypeName(type), from);
    }
}