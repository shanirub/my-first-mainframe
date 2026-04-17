// ─────────────────────────────────────────────────────────────────────────────
// MCU #5 — I/O Controller
// File: code/mcu5-io-controller/src/config.h
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "shared_config.h"

#define I2C_ADDRESS     ADDR_IO_CONTROLLER

// OLED private bus (U8g2 software I2C — ESP32-C3 SuperMini pins)
#define OLED_SDA_PIN    3
#define OLED_SCL_PIN    10
