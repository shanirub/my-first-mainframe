// ─────────────────────────────────────────────────────────────────────────────
// OledDisplayWroom — implementation
// u8g2 C API + u8g2-hal-esp-idf, I2C_NUM_1, GPIO16/17
// ─────────────────────────────────────────────────────────────────────────────
#include "oled_display_wroom.h"
#include "esp_log.h"

static const char* TAG = "OledWroom";

OledDisplayWroom::OledDisplayWroom(uint8_t sdaPin, uint8_t sclPin)
    : _sdaPin(sdaPin), _sclPin(sclPin)
{
}

bool OledDisplayWroom::begin()
{
    // Configure HAL pin mapping.
    // u8g2_esp32_hal_init() calls i2c_driver_install() on I2C_NUM_1.
    u8g2_esp32_hal_t hal = U8G2_ESP32_HAL_DEFAULT;
    hal.bus.i2c.sda = (gpio_num_t)_sdaPin;
    hal.bus.i2c.scl = (gpio_num_t)_sclPin;
    u8g2_esp32_hal_init(hal);

    // Select display: SSD1306, 128x64, I2C, full framebuffer (_f suffix).
    // Full framebuffer: entire frame built in RAM then pushed in one transfer.
    // Callbacks: byte-level I2C transport + GPIO/delay.
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &_u8g2,
        U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb
    );

    // I2C address must be left-shifted by 1 (u8g2 convention).
    // 0x3C << 1 = 0x78
    u8x8_SetI2CAddress(&_u8g2.u8x8, 0x78);

    // Send controller init sequence, then wake from power-save (display off by default).
    u8g2_InitDisplay(&_u8g2);
    u8g2_SetPowerSave(&_u8g2, 0);

    ESP_LOGI(TAG, "OLED init OK — SDA GPIO%d SCL GPIO%d", _sdaPin, _sclPin);
    return true;
}

void OledDisplayWroom::showStatus(
    const char* line1,
    const char* line2,
    const char* line3,
    const char* line4)
{
    // ClearBuffer: zero RAM framebuffer.
    // SetFont: 6x10px proportional font, full ASCII (tf = transparent background).
    // DrawStr: (x, y) where y is the text baseline, not the top edge.
    // SendBuffer: push framebuffer to display over I2C.
    u8g2_ClearBuffer(&_u8g2);
    u8g2_SetFont(&_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&_u8g2, 0, 12, line1);
    u8g2_DrawStr(&_u8g2, 0, 26, line2);
    u8g2_DrawStr(&_u8g2, 0, 40, line3);
    u8g2_DrawStr(&_u8g2, 0, 54, line4);
    u8g2_SendBuffer(&_u8g2);
}

void OledDisplayWroom::showError(const char* message)
{
    u8g2_ClearBuffer(&_u8g2);
    u8g2_SetFont(&_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&_u8g2, 0, 12, "** ERROR **");
    u8g2_DrawStr(&_u8g2, 0, 26, message);
    u8g2_SendBuffer(&_u8g2);
}