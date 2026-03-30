#include "oled_display.h"

OledDisplay::OledDisplay(uint8_t sdaPin, uint8_t sclPin)
    : _bus(0),
      _display(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &_bus, -1),
      _sdaPin(sdaPin),
      _sclPin(sclPin) {}

bool OledDisplay::begin() {
    _bus.begin(_sdaPin, _sclPin);
    _bus.setClock(100000);

    if (!_display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println("[OLED] FAIL: display.begin() returned false");
        return false;
    }

    Serial.println("[OLED] PASS: initialized");
    _display.clearDisplay();
    _display.display();
    return true;
}

void OledDisplay::showStatus(
    const char* line1,
    const char* line2,
    const char* line3,
    const char* line4
) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    _display.setCursor(0, 0);  _display.println(line1);
    _display.setCursor(0, 16); _display.println(line2);
    _display.setCursor(0, 32); _display.println(line3);
    _display.setCursor(0, 48); _display.println(line4);

    _display.display();
}

void OledDisplay::showError(const char* message) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 0);
    _display.println("*** ERROR ***");
    _display.setCursor(0, 16);
    _display.println(message);
    _display.display();
}