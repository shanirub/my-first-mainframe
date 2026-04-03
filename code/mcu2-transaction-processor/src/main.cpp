// mcu2-transaction-processor/src/main.cpp
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "oled_display.h"

TwoWire sharedBus = TwoWire(0);
OledDisplay oled(OLED_SDA_PIN, OLED_SCL_PIN);

void onReceive(int numBytes) {
    char buf[32] = {0};
    int i = 0;
    while (sharedBus.available() && i < 31) {
        buf[i++] = sharedBus.read();
    }
    Serial.print("[MCU2] Received ");
    Serial.print(numBytes);
    Serial.print(" bytes: ");
    Serial.println(buf);
    oled.showStatus("TRANSACTION", "PROCESSOR", "RX:", buf);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!oled.begin()) {
        Serial.println("[MCU2] FAIL: OLED init failed");
    } else {
        oled.showStatus("TRANSACTION", "PROCESSOR", "READY", "");
        Serial.println("[MCU2] OLED OK");
    }

    sharedBus.begin(I2C_ADDRESS, SHARED_SDA_PIN, SHARED_SCL_PIN, 0);
    sharedBus.onReceive(onReceive);
    Serial.println("[MCU2] Slave ready, listening on 0x09...");
}

void loop() {
    delay(100);
}
