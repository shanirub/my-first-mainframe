// ─────────────────────────────────────────────────────────────────────────────
// MCU #3 — Database Controller
// File: code/mcu3-database-controller/src/config.h
// Board: ESP32 DevKit (ESP32-D0WDQ6, 38-pin) — see ADR-008
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "shared_config.h"

#define I2C_ADDRESS     ADDR_DATABASE_CONTROLLER

// OLED private bus (U8g2 software I2C — ESP32 DevKit pins)
// GPIO3/10 are not safe on ESP32 DevKit (RX0 and flash SPI).
// GPIO16/17 are clean general-purpose pins with no special functions.
#define OLED_SDA_PIN    16
#define OLED_SCL_PIN    17

// SD card (SPI — ESP32 DevKit default SPI2 pins)
#define SD_MOSI_PIN     23
#define SD_MISO_PIN     19
#define SD_SCK_PIN      18
#define SD_CS_PIN       5
