#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// OledDisplayWroom — SSD1306 128x64 OLED driver for ESP32-WROOM-32 (MCU #3).
// Pure ESP-IDF, no Arduino layer, no u8g2-hal-esp-idf.
//
// Transport: u8g2 C API with custom callbacks using new I2C master driver
//   (driver/i2c_master.h) on I2C_NUM_1, GPIO16=SDA, GPIO17=SCL.
//
// Replaces u8g2-hal-esp-idf which used the legacy driver (driver/i2c.h) and
// conflicted with the new slave driver (driver/i2c_slave.h) on I2C_NUM_0.
// Old and new I2C drivers cannot coexist in the same binary — see ADR-010.
//
// I2C address: 0x3C — passed directly to i2c_master_bus_add_device().
// The new master driver handles the R/W bit shift internally.
// Do NOT left-shift to 0x78 (that was required by the old HAL only).
//
// Singleton pattern: static callbacks need access to member handles.
// Exactly one OledDisplayWroom instance may exist — enforced by _instance.
// ─────────────────────────────────────────────────────────────────────────────

#include <stdint.h>
#include "u8g2.h"
#include "driver/i2c_master.h"

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
    uint8_t                _sdaPin;
    uint8_t                _sclPin;
    u8g2_t                 _u8g2;         // u8g2 C struct — owns all display state
    i2c_master_bus_handle_t _busHandle;   // I2C_NUM_1 master bus
    i2c_master_dev_handle_t _devHandle;   // SSD1306 device on the bus

    // Singleton pointer — static callbacks reach member state through this.
    static OledDisplayWroom* _instance;

    // u8g2 callbacks — must be static (plain C function pointers).
    // byte_cb:       buffers bytes, transmits on END_TRANSFER via i2c_master_transmit()
    // gpio_delay_cb: handles timing delays, no-op for GPIO (no RST pin)
    static uint8_t byte_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);
    static uint8_t gpio_delay_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);
};