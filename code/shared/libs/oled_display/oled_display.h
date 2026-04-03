#pragma once
#include <U8g2lib.h>

#define OLED_SCREEN_WIDTH  128
#define OLED_SCREEN_HEIGHT 64
#define OLED_ADDRESS       0x3C

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
    U8G2_SSD1306_128X64_NONAME_F_SW_I2C _display;
};