// mcu3-database-controller/src/main.cpp
#include <Arduino.h>
#include "config.h"
#include "oled_display.h"
#include "shared_bus.h"

SharedBus sharedBus;
OledDisplay oled(OLED_SDA_PIN, OLED_SCL_PIN);

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!oled.begin()) {
        Serial.println("[MCU3] FAIL: OLED init failed");
    } else {
        oled.showStatus("DATABASE", "CONTROLLER", "READY", "");
        Serial.println("[MCU3] OLED OK");
    }

    sharedBus.beginSlave(I2C_ADDRESS);
    Serial.println("[MCU3] Slave ready, listening on 0x0A (DATABASE CONTROLLER)...");
}

void loop() {
    char buf[32];
    if (sharedBus.poll(buf, sizeof(buf))) {
        Serial.print("[MCU3] Received: ");
        Serial.println(buf);
        oled.showStatus("DATABASE", "CONTROLLER", "RX:", buf);
    }
    delay(10);
}
