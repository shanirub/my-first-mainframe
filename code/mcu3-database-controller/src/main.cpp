#include <Arduino.h>
#include "esp_idf_version.h"
#include "esp_log.h"
#include "config.h"
#include "oled_display_wroom.h"

OledDisplayWroom oled(OLED_SDA_PIN, OLED_SCL_PIN);

void setup() {
    esp_log_level_set("gpio", ESP_LOG_WARN);  // suppress GPIO INFO spam

    Serial.begin(921600);
    delay(1000);
    Serial.println("[MCU3-0.2] OLED smoke test — pioarduino HW_I2C");
    Serial.printf("[MCU3-0.2] IDF: %s  OLED SDA=GPIO%d SCL=GPIO%d\n",
        esp_get_idf_version(), OLED_SDA_PIN, OLED_SCL_PIN);

    Serial.println("[MCU3-0.2] calling oled.begin()...");
    // If this hangs: HW_I2C is broken under pioarduino — use SW_I2C fallback
    bool ok = oled.begin();

    if (ok) {
        Serial.println("[MCU3-0.2] PASS: HW_I2C working");
        oled.showStatus("MCU3-0.2", "OLED HW_I2C", "PASS", "");
    } else {
        Serial.println("[MCU3-0.2] FAIL: begin() returned false — SW_I2C fallback needed");
    }
}

void loop() {
    delay(5000);
    Serial.println("[MCU3-0.2] alive");
}