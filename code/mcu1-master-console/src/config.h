// ─────────────────────────────────────────────────────────────────────────────
// MCU #1 — Master Console
// File: code/mcu1-master-console/src/config.h
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "shared_config.h"

#define I2C_ADDRESS     ADDR_MASTER_CONSOLE

// OLED private bus (U8g2 software I2C — ESP32-C3 SuperMini pins)
#define OLED_SDA_PIN    3
#define OLED_SCL_PIN    10