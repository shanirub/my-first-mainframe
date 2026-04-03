#include <Arduino.h>
#include <Wire.h>
#include "config.h"

TwoWire sharedBus = TwoWire(0);

void setup() {
    Serial.begin(115200);
    delay(1000);
    sharedBus.begin(SHARED_SDA_PIN, SHARED_SCL_PIN);
    sharedBus.setClock(SHARED_I2C_FREQ);
    Serial.println("[MCU1] Master ready, will send to 0x09...");
}

void loop() {
    sharedBus.beginTransmission(ADDR_TRANSACTION_PROCESSOR);
    sharedBus.write("HELLO FROM MCU1");
    uint8_t result = sharedBus.endTransmission();

    if (result == 0) {
        Serial.println("[MCU1] SEND OK — MCU2 acknowledged");
    } else {
        Serial.print("[MCU1] SEND FAILED — error code: ");
        Serial.println(result);
    }

    delay(2000);
}