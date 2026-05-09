// ─────────────────────────────────────────────────────────────────────────────
// MCU #3 — Database Controller
// File: code/mcu3-database-controller/src/config.h
// Board: ESP32-WROOM-32 DevKit (38-pin, CP2102) — see ADR-008, ADR-009
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "shared_config.h"

#define I2C_ADDRESS     ADDR_DATABASE_CONTROLLER

// OLED private bus (U8g2 software I2C)
#define OLED_SDA_PIN    16
#define OLED_SCL_PIN    17

// UART1 — connection to Raspberry Pi DB backend (see ADR-009)
// RPi GPIO14 (TX) → MCU GPIO19 (RX)
// RPi GPIO15 (RX) → MCU GPIO18 (TX)
#define UART_TX_PIN     18
#define UART_RX_PIN     19