#include <Arduino.h>
#include <Wire.h>
#include "config.h"

TwoWire sharedBus = TwoWire(0);

void onReceive(int numBytes) {
    Serial.print("[MCU2] Received ");
    Serial.print(numBytes);
    Serial.print(" bytes: ");
    while (sharedBus.available()) {
        char c = sharedBus.read();
        Serial.print(c);
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    sharedBus.begin(I2C_ADDRESS, SHARED_SDA_PIN, SHARED_SCL_PIN);
    sharedBus.onReceive(onReceive);
    Serial.println("[MCU2] Slave ready, listening on 0x09...");
}

void loop() {
    delay(100);
}