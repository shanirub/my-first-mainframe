// mcu4-job-scheduler/src/main.cpp
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
        Serial.println("[MCU4] FAIL: OLED init failed");
    } else {
        oled.showStatus("JOB", "SCHEDULER", "READY", "");
        Serial.println("[MCU4] OLED OK");
    }

    sharedBus.beginSlave(I2C_ADDRESS);
    Serial.println("[MCU4] Slave ready, listening on 0x0B...");
}

void loop() {
    char buf[32];
    if (sharedBus.poll(buf, sizeof(buf))) {
        Serial.print("[MCU4] Received: ");
        Serial.println(buf);
        oled.showStatus("JOB", "SCHEDULER", "RX:", buf);
    }
    delay(10);
}
