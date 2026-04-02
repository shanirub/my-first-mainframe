#pragma once

// ─── Shared Inter-MCU I2C Bus ───────────────────────────────
#define SHARED_SDA_PIN      8
#define SHARED_SCL_PIN      9
#define SHARED_I2C_FREQ     400000  // 400kHz Fast Mode

// ─── Private OLED I2C Bus ───────────────────────────────────
#define OLED_SDA_PIN        3
#define OLED_SCL_PIN        10

// ─── MCU I2C Addresses on Shared Bus ────────────────────────
#define ADDR_MASTER_CONSOLE         0x08
#define ADDR_TRANSACTION_PROCESSOR  0x09
#define ADDR_DATABASE_CONTROLLER    0x0A
#define ADDR_JOB_SCHEDULER          0x0B
#define ADDR_IO_CONTROLLER          0x0C