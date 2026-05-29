#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// OledDisplayWroom — SSD1306 128x64 OLED driver for ESP32-WROOM-32.
// Pure ESP-IDF, no Arduino layer.
//
// Transport: u8g2-hal-esp-idf on I2C_NUM_1 (GPIO16=SDA, GPIO17=SCL).
// The HAL uses the legacy i2c driver (driver/i2c.h) internally — safe in
// IDF 5.4.0 as long as no other code calls i2c_new_master_bus() on I2C_NUM_1.
//
// HAL hardcodes 50kHz bus speed. Acceptable for a 500ms-refresh display.
//
// I2C address: 0x3C (left-shifted to 0x78 per u8g2 convention).
// ─────────────────────────────────────────────────────────────────────────────
#include <stdint.h>
#include "u8g2.h"
// Tell C++ compiler these are C functions — do not mangle their names
#ifdef __cplusplus
extern "C" {
#endif
#include "u8g2_esp32_hal.h"
#ifdef __cplusplus
}
#endif

class OledDisplayWroom {
public:
    OledDisplayWroom(uint8_t sdaPin, uint8_t sclPin);
    bool begin();
    void showStatus(
        const char* line1,
        const char* line2,
        const char* line3,
        const char* line4
    );
    void showError(const char* message);

private:
    uint8_t  _sdaPin;
    uint8_t  _sclPin;
    u8g2_t   _u8g2;    // u8g2 C struct — owns all display state
};