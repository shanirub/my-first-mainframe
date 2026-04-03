#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "oled_display.h"

TwoWire sharedBus = TwoWire(0);
OledDisplay oled(OLED_SDA_PIN, OLED_SCL_PIN);

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!oled.begin()) {
        Serial.println("[MCU1] FAIL: OLED init failed");
    } else {
        oled.showStatus("MASTER CONSOLE", "STARTING", "", "");
        Serial.println("[MCU1] OLED OK");
    }

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
        oled.showStatus("MASTER CONSOLE", "TX OK", "-> MCU2", "");
    } else {
        Serial.print("[MCU1] SEND FAILED — error code: ");
        Serial.println(result);
        oled.showStatus("MASTER CONSOLE", "TX FAIL", "-> MCU2", "");
    }

    delay(2000);
}
