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
        Serial.println("[MCU1] FAIL: OLED init failed");
    } else {
        oled.showStatus("MASTER CONSOLE", "STARTING", "", "");
        Serial.println("[MCU1] OLED OK");
    }

    sharedBus.beginMaster();
    Serial.println("[MCU1] Master ready, will send to 0x0C (MCU5)...");
}

void loop() {
    BusError err = sharedBus.send(ADDR_IO_CONTROLLER, "HELLO FROM MCU1");

    if (err == BusError::OK) {
        Serial.println("[MCU1] SEND OK — MCU5 acknowledged");
        oled.showStatus("MASTER CONSOLE", "TX OK", "-> MCU5", "");
    } else if (err == BusError::NOT_FOUND) {
        Serial.println("[MCU1] SEND FAILED — MCU5 not found");
        oled.showStatus("MASTER CONSOLE", "TX FAIL", "NOT FOUND", "");
    } else if (err == BusError::TIMEOUT) {
        Serial.println("[MCU1] SEND FAILED — timeout");
        oled.showStatus("MASTER CONSOLE", "TX FAIL", "TIMEOUT", "");
    } else {
        Serial.println("[MCU1] SEND FAILED — bus fault");
        oled.showStatus("MASTER CONSOLE", "TX FAIL", "BUS FAULT", "");
    }

    delay(2000);
}
