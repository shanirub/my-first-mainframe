#pragma once

// ─── Shared Inter-MCU I2C Bus ───────────────────────────────
// These pins are identical on all MCUs — hub wiring depends on them.
#define SHARED_SDA_PIN      8
#define SHARED_SCL_PIN      9
#define SHARED_I2C_FREQ     400000  // 400kHz Fast Mode

// ─── MCU I2C Addresses on Shared Bus ────────────────────────
#define ADDR_MASTER_CONSOLE         0x08
#define ADDR_TRANSACTION_PROCESSOR  0x09
#define ADDR_DATABASE_CONTROLLER    0x0A
#define ADDR_JOB_SCHEDULER          0x0B
#define ADDR_IO_CONTROLLER          0x0C

// ─── FreeRTOS Task Stack Sizes ──────────────────────────────
// Units are bytes (ESP32 xTaskCreate takes bytes, not words).
// Sender and logic tasks use ArduinoJson (JsonDocument on stack ~200-300 bytes)
// plus Serial printf — 4096 gives comfortable headroom.
// Receiver and OLED tasks are shallow: semaphore block, memcpy, queue send.
// 2048 is sufficient. Tune with uxTaskGetStackHighWaterMark() if RAM is tight.
#define STACK_SIZE_SENDER    4096
#define STACK_SIZE_RECEIVER  2048
#define STACK_SIZE_LOGIC     4096
#define STACK_SIZE_OLED      2048

// ─── Heartbeat Timing ───────────────────────────────────────
#define HEARTBEAT_INTERVAL_MS       10000   // interval between heartbeat cycles (ms)
#define HEARTBEAT_ACK_TIMEOUT_MS    30000   // slave inactive if no ACK within this window (3× interval)

// ─── Note: OLED pins are NOT defined here ───────────────────
// OLED_SDA_PIN and OLED_SCL_PIN differ per MCU board.
// MCUs #1/#2/#4/#5 (ESP32-C3 SuperMini): GPIO3=SDA, GPIO10=SCL
// MCU #3 (ESP32 DevKit):                 GPIO16=SDA, GPIO17=SCL
// Each MCU defines these in its own config.h.