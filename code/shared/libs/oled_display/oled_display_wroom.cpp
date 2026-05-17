#include "oled_display_wroom.h"

// U8G2_SSD1306_128X64_NONAME_F_HW_I2C constructor arg order:
//   (rotation, reset, SCL, SDA)
// U8X8_PIN_NONE = no reset pin connected.
// SCL and SDA pins are passed to remap Wire (bus 0) away from its defaults
// (GPIO21/22 on WROOM) to our OLED pins (GPIO17=SCL, GPIO16=SDA).
// This does NOT conflict with Wire1 (bus 1) on GPIO8/9 (shared bus).
OledDisplayWroom::OledDisplayWroom(uint8_t sdaPin, uint8_t sclPin)
    : _display(U8G2_R0, U8X8_PIN_NONE, sclPin, sdaPin),
      _sdaPin(sdaPin),
      _sclPin(sclPin) {}

bool OledDisplayWroom::begin() {
    if (!_display.begin()) {
        Serial.println("[OLED-WROOM] FAIL: display.begin() returned false");
        return false;
    }
    Serial.println("[OLED-WROOM] PASS: initialized via hardware I2C");
    _display.clearBuffer();
    _display.sendBuffer();
    return true;
}

void OledDisplayWroom::showStatus(
    const char* line1,
    const char* line2,
    const char* line3,
    const char* line4
) {
    _display.clearBuffer();
    _display.setFont(u8g2_font_6x10_tf);
    _display.drawStr(0, 10, line1);
    _display.drawStr(0, 26, line2);
    _display.drawStr(0, 42, line3);
    _display.drawStr(0, 58, line4);
    _display.sendBuffer();
}

void OledDisplayWroom::showError(const char* message) {
    _display.clearBuffer();
    _display.setFont(u8g2_font_6x10_tf);
    _display.drawStr(0, 10, "*** ERROR ***");
    _display.drawStr(0, 26, message);
    _display.sendBuffer();
}