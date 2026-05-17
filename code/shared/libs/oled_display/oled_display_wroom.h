#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// OledDisplayWroom — hardware I2C OLED driver for ESP32-WROOM-32.
//
// Uses U8g2 HW_I2C constructor (TwoWire bus 0, remapped to GPIO16/17).
// This avoids software I2C bit-banging which causes TG1WDT on WROOM due to
// the dual-core scheduler's strict per-core idle task monitoring.
//
// TwoWire bus 1 (Wire1) is reserved for the shared inter-MCU bus on GPIO8/9.
// TwoWire bus 0 (Wire) is used here, remapped to OLED_SDA=GPIO16, SCL=GPIO17.
//
// Same interface as OledDisplay (oled_display.h) — drop-in replacement.
// Selected via #ifdef MCU_BOARD_WROOM in consuming code.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <U8g2lib.h>

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
    // U8G2_SSD1306_128X64_NONAME_F_HW_I2C:
    //   F = full framebuffer (128x64 / 8 = 1024 bytes in RAM)
    //   HW_I2C = uses hardware I2C peripheral (Wire / TwoWire bus 0)
    //   Constructor arg order: (rotation, reset, SCL, SDA)
    //   Pins passed here remap Wire to GPIO16/17 instead of defaults.
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C _display;

    uint8_t _sdaPin;
    uint8_t _sclPin;
};