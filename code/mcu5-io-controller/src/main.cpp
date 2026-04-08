#include <Arduino.h>
#include "config.h"
#include "oled_display.h"
#include "shared_bus.h"
#include "message_protocol.h"

// config.h on each MCU defines I2C_ADDRESS differently:
// MCU #2: ADDR_TRANSACTION_PROCESSOR (0x09)
// MCU #3: ADDR_DATABASE_CONTROLLER   (0x0A)
// MCU #4: ADDR_JOB_SCHEDULER         (0x0B)
// MCU #5: ADDR_IO_CONTROLLER         (0x0C)

OledDisplay oled(OLED_SDA_PIN, OLED_SCL_PIN);
SharedBus   sharedBus;

char     rxBuf[256];
char     lastMsgType[20] = "none";
uint32_t msgCount        = 0;

void sendAck(uint8_t to) {
    JsonDocument doc = MessageProtocol::createEnvelope(
        I2C_ADDRESS, to, MsgType::HEARTBEAT_ACK
    );
    doc[Field::PAYLOAD][Field::STATUS] = Status::OK;
    doc[Field::PAYLOAD][Field::UPTIME] = millis();

    char buf[256];
    if (!MessageProtocol::serialize(doc, buf, sizeof(buf))) return;

    BusError err = sharedBus.send(to, buf);
    Serial.printf("[0x%02X] ACK → 0x%02X: %s\n",
        I2C_ADDRESS, to, err == BusError::OK ? "OK" : "FAILED");
}

void updateOled() {
    char line2[20], line3[20], line4[20];
    snprintf(line2, sizeof(line2), "Addr: 0x%02X", I2C_ADDRESS);
    snprintf(line3, sizeof(line3), "Last: %s", lastMsgType);
    snprintf(line4, sizeof(line4), "Count: %lu", msgCount);
    oled.showStatus("SLAVE READY", line2, line3, line4);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.printf("[0x%02X] Slave starting...\n", I2C_ADDRESS);

    if (!oled.begin()) {
        Serial.println("OLED failed");
    }

    sharedBus.beginSlave(I2C_ADDRESS);
    Serial.printf("[0x%02X] SharedBus slave ready\n", I2C_ADDRESS);

    updateOled();
}

void loop() {
    if (sharedBus.poll(rxBuf, sizeof(rxBuf))) {
        JsonDocument doc     = MessageProtocol::parse(rxBuf);
        ValidationResult res = MessageProtocol::validate(doc);

        if (!res.valid) {
            Serial.printf("[0x%02X] Invalid: %s\n", I2C_ADDRESS, res.detail);
            return;
        }

        uint8_t type = doc[Field::TYPE];
        uint8_t from = doc[Field::FROM];
        msgCount++;

        Serial.printf("[0x%02X] Received %s from 0x%02X (#%lu)\n",
            I2C_ADDRESS,
            MessageProtocol::msgTypeName(type),
            from,
            msgCount);

        // Store last message type for OLED
        strncpy(lastMsgType,
            MessageProtocol::msgTypeName(type),
            sizeof(lastMsgType) - 1);

        // Handle by type
        switch (type) {
            case MsgType::HEARTBEAT:
                sendAck(from);
                break;
            default:
                Serial.printf("[0x%02X] Unhandled type: %d\n",
                    I2C_ADDRESS, type);
                break;
        }

        updateOled();
    }
}
