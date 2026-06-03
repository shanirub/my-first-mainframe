// ─────────────────────────────────────────────────────────────────────────────
// OledDisplayWroom — implementation
// New I2C master driver (driver/i2c_master.h), I2C_NUM_1, GPIO16/17.
// Callbacks modelled directly on Espressif's official i2c_u8g2 example
// (examples/peripherals/i2c/i2c_u8g2, 2025, Unlicense/CC0).
// ─────────────────────────────────────────────────────────────────────────────
#include "oled_display_wroom.h"
#include "esp_log.h"
#include "esp_rom_sys.h"   // esp_rom_delay_us()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "OledWroom";

// ── Constants ─────────────────────────────────────────────────────────────────
#define OLED_I2C_PORT       I2C_NUM_1
#define OLED_I2C_ADDR       0x3C    // raw 7-bit address — new driver handles R/W bit
#define OLED_I2C_FREQ_HZ    400000  // 400kHz Fast Mode — new driver configurable
#define OLED_I2C_TIMEOUT_MS 1000
#define OLED_TX_BUF_SIZE    132     // control byte + 128 data bytes + margin

// ── Static member definitions ─────────────────────────────────────────────────
OledDisplayWroom* OledDisplayWroom::_instance = nullptr;

// ── Constructor ───────────────────────────────────────────────────────────────
OledDisplayWroom::OledDisplayWroom(uint8_t sdaPin, uint8_t sclPin)
    : _sdaPin(sdaPin)
    , _sclPin(sclPin)
    , _busHandle(nullptr)
    , _devHandle(nullptr)
{
    _instance = this;
}

// ── byte_cb ───────────────────────────────────────────────────────────────────
// Called by u8g2 for all I2C byte-level operations.
// Buffers bytes on SEND, transmits in one call on END_TRANSFER.
// Adds device to bus on BYTE_INIT (called once by u8g2_InitDisplay).
//
// Modelled on Espressif i2c_u8g2 example — u8x8_byte_i2c_cb().
uint8_t OledDisplayWroom::byte_cb(u8x8_t* u8x8, uint8_t msg,
                                   uint8_t arg_int, void* arg_ptr)
{
    static uint8_t buf[OLED_TX_BUF_SIZE];
    static uint8_t buf_idx = 0;

    if (_instance == nullptr) return 0;

    switch (msg) {
    case U8X8_MSG_BYTE_INIT:
        // Add SSD1306 device to the bus.
        // Called once during u8g2_InitDisplay().
        {
            i2c_device_config_t dev_cfg = {
                .dev_addr_length      = I2C_ADDR_BIT_LEN_7,
                .device_address       = OLED_I2C_ADDR,
                .scl_speed_hz         = OLED_I2C_FREQ_HZ,
                .scl_wait_us          = 0,
                .flags = {
                    .disable_ack_check = false,
                },
            };
            esp_err_t err = i2c_master_bus_add_device(
                _instance->_busHandle, &dev_cfg, &_instance->_devHandle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "i2c_master_bus_add_device failed: %s",
                         esp_err_to_name(err));
                return 0;
            }
            ESP_LOGI(TAG, "OLED device added — addr 0x%02X @ %dHz",
                     OLED_I2C_ADDR, OLED_I2C_FREQ_HZ);
        }
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        // New transfer — reset buffer.
        buf_idx = 0;
        break;

    case U8X8_MSG_BYTE_SET_DC:
        // D/C control — for I2C displays this is encoded in the data stream,
        // not a GPIO pin. No-op.
        break;

    case U8X8_MSG_BYTE_SEND:
        // Accumulate bytes into buffer.
        // arg_int = byte count, arg_ptr = byte array.
        for (uint8_t i = 0; i < arg_int; i++) {
            if (buf_idx < OLED_TX_BUF_SIZE) {
                buf[buf_idx++] = ((uint8_t*)arg_ptr)[i];
            }
        }
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:
        // Transmit entire buffer in one I2C transaction.
        if (buf_idx > 0 && _instance->_devHandle != nullptr) {
            esp_err_t err = i2c_master_transmit(
                _instance->_devHandle, buf, buf_idx, OLED_I2C_TIMEOUT_MS);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "i2c_master_transmit failed: %s",
                         esp_err_to_name(err));
                return 0;
            }
        }
        break;

    default:
        return 0;
    }
    return 1;
}

// ── gpio_delay_cb ─────────────────────────────────────────────────────────────
// Handles timing delays required by u8g2 during init and transfers.
// No physical GPIO needed — SSD1306 has no RST pin in our wiring.
//
// Modelled on Espressif i2c_u8g2 example — u8x8_gpio_delay_cb().
uint8_t OledDisplayWroom::gpio_delay_cb(u8x8_t* u8x8, uint8_t msg,
                                         uint8_t arg_int, void* arg_ptr)
{
    switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        // No GPIO to configure — hardware I2C, no RST pin.
        break;

    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int));
        break;

    case U8X8_MSG_DELAY_10MICRO:
        esp_rom_delay_us(arg_int * 10);
        break;

    case U8X8_MSG_DELAY_100NANO:
        // Minimal delay — single NOP sufficient on 160MHz ESP32.
        __asm__ __volatile__("nop");
        break;

    case U8X8_MSG_DELAY_I2C:
        // I2C timing hint for software I2C — no-op for hardware I2C.
        // Formula from Espressif example: 5us / arg_int.
        esp_rom_delay_us(5 / arg_int);
        break;

    case U8X8_MSG_GPIO_RESET:
        // No RST pin wired — no-op.
        break;

    default:
        return 0;
    }
    return 1;
}

// ── begin() ───────────────────────────────────────────────────────────────────
bool OledDisplayWroom::begin()
{
    // Create I2C_NUM_1 master bus.
    // glitch_ignore_cnt = 7: standard filter value from Espressif example.
    // enable_internal_pullup: true — acceptable at 400kHz with short traces.
    //   If display is unreliable, add external 4.7kΩ pull-ups and set false.
    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port             = OLED_I2C_PORT,
        .sda_io_num           = (gpio_num_t)_sdaPin,
        .scl_io_num           = (gpio_num_t)_sclPin,
        .clk_source           = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt    = 7,
        .intr_priority        = 0,
        .trans_queue_depth    = 0,  // 0 = synchronous mode
        .flags = {
            .enable_internal_pullup = true,
        },
    };

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &_busHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return false;
    }

    // Configure u8g2: SSD1306, 128x64, full framebuffer (_f), I2C.
    // Device is added to bus inside byte_cb on U8X8_MSG_BYTE_INIT,
    // which u8g2_InitDisplay() triggers automatically.
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &_u8g2,
        U8G2_R0,
        byte_cb,
        gpio_delay_cb
    );

    u8g2_InitDisplay(&_u8g2);
    u8g2_SetPowerSave(&_u8g2, 0);  // wake from sleep

    ESP_LOGI(TAG, "OLED init OK — SDA GPIO%d SCL GPIO%d @ %dHz",
             _sdaPin, _sclPin, OLED_I2C_FREQ_HZ);
    return true;
}

// ── showStatus() ──────────────────────────────────────────────────────────────
void OledDisplayWroom::showStatus(
    const char* line1,
    const char* line2,
    const char* line3,
    const char* line4)
{
    u8g2_ClearBuffer(&_u8g2);
    u8g2_SetFont(&_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&_u8g2, 0, 12, line1);
    u8g2_DrawStr(&_u8g2, 0, 26, line2);
    u8g2_DrawStr(&_u8g2, 0, 40, line3);
    u8g2_DrawStr(&_u8g2, 0, 54, line4);
    u8g2_SendBuffer(&_u8g2);
}

// ── showError() ───────────────────────────────────────────────────────────────
void OledDisplayWroom::showError(const char* message)
{
    u8g2_ClearBuffer(&_u8g2);
    u8g2_SetFont(&_u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&_u8g2, 0, 12, "** ERROR **");
    u8g2_DrawStr(&_u8g2, 0, 26, message);
    u8g2_SendBuffer(&_u8g2);
}