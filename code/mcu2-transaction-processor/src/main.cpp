// mcu2-transaction-processor/src/main.cpp
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
        Serial.println("[MCU2] FAIL: OLED init failed");
    } else {
        oled.showStatus("TRANSACTION", "PROCESSOR", "READY", "");
        Serial.println("[MCU2] OLED OK");
    }

    sharedBus.beginSlave(I2C_ADDRESS);
    Serial.println("[MCU2] Slave ready, listening on 0x09...");
}

void loop() {
    char buf[32];
    if (sharedBus.poll(buf, sizeof(buf))) {
        Serial.print("[MCU2] Received: ");
        Serial.println(buf);
        oled.showStatus("TRANSACTION", "PROCESSOR", "RX:", buf);
    }
    delay(10);
}
