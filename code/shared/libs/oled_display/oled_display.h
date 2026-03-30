#pragma once
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_SCREEN_WIDTH 128
#define OLED_SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

class OledDisplay {
public:
    OledDisplay(uint8_t sdaPin, uint8_t sclPin);
    bool begin();
    void showStatus(
        const char* line1,  // e.g. "MASTER CONSOLE"
        const char* line2,  // e.g. "READY"
        const char* line3,  // e.g. "5 subsystems"
        const char* line4   // e.g. "Last: DEPOSIT"
    );
    void showError(const char* message);

private:
    TwoWire _bus;
    Adafruit_SSD1306 _display;
    uint8_t _sdaPin;
    uint8_t _sclPin;
};