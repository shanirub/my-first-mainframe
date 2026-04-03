#include <Arduino.h>
#include "oled_display.h"

#define SDA_PIN 3
#define SCL_PIN 10

OledDisplay oled(SDA_PIN, SCL_PIN);

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("MCU1 Master Console starting...");

    if (!oled.begin()) {
        Serial.println("FAIL: OLED init failed");
        while (true) {
            Serial.println("[HEARTBEAT] stuck - OLED failed");
            delay(3000);
        }
    }

    oled.showStatus(
        "MASTER CONSOLE",
        "READY",
        "Subsystems: 0",
        "Last: none"
    );

    Serial.println("PASS: display showing");
}

void loop() {
    Serial.println("[HEARTBEAT] running");
    delay(3000);
}