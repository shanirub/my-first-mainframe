#include "oled_display.h"

// U8g2 SW_I2C constructor arg order: (rotation, SCL, SDA, reset)
// Note: SCL comes before SDA — opposite of Wire convention
OledDisplay::OledDisplay(uint8_t sdaPin, uint8_t sclPin)
    : _display(U8G2_R0, sclPin, sdaPin, U8X8_PIN_NONE) {}

bool OledDisplay::begin() {
    if (!_display.begin()) {
        Serial.println("[OLED] FAIL: display.begin() returned false");
        return false;
    }
    Serial.println("[OLED] PASS: initialized");
    _display.clearBuffer();
    _display.sendBuffer();
    return true;
}

void OledDisplay::showStatus(
    const char* line1,
    const char* line2,
    const char* line3,
    const char* line4
) {
    _display.clearBuffer();
    _display.setFont(u8g2_font_6x10_tf);
    // U8g2 y-coordinates are text baseline, not top-left.
    // Font height is 10px; baselines at 10/26/42/58 match the
    // original 4-line layout at y=0/16/32/48.
    _display.drawStr(0, 10, line1);
    _display.drawStr(0, 26, line2);
    _display.drawStr(0, 42, line3);
    _display.drawStr(0, 58, line4);
    _display.sendBuffer();
}

void OledDisplay::showError(const char* message) {
    _display.clearBuffer();
    _display.setFont(u8g2_font_6x10_tf);
    _display.drawStr(0, 10, "*** ERROR ***");
    _display.drawStr(0, 26, message);
    _display.sendBuffer();
}