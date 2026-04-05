# ADR-002: Software I2C (U8g2) for OLED displays

## Status
Accepted

## Context
Each MCU needs two simultaneous I2C buses:
- Shared inter-MCU bus (GPIO8/GPIO9)
- Private OLED display bus (GPIO3/GPIO10)

ESP32-C3 has only one hardware I2C peripheral (SOC_I2C_NUM = 1), confirmed
by reading Espressif's soc_caps.h source. The comment in that file says
"ESP32-C3 have 2 I2C" but the define is SOC_I2C_NUM = 1 — the comment is
a copy-paste error from ESP32. The define is the ground truth.

TwoWire(1) constructs without error but begin() silently fails when the HAL
rejects bus index 1 (1 >= SOC_I2C_NUM). This was confirmed by device monitor
showing null buffer errors.

Adafruit SSD1306 constructor takes TwoWire* — incompatible with SoftWire
which does not inherit from TwoWire. Using SoftWire would require modifying
Adafruit's library which we do not own.

## Decision
Use U8g2 library with its built-in software I2C for all OLED displays.
Hardware TwoWire(0) is reserved exclusively for the shared inter-MCU bus.

## Reasoning
- U8g2 bundles software I2C as a first-class feature — no glue code needed
- Single dependency, no compatibility concerns between libraries
- OLEDs update infrequently (a few times per second) — software I2C at
  100kHz is sufficient, CPU overhead is negligible
- Shared bus (hardware I2C) keeps full 400kHz performance unaffected
- Software I2C can run on any GPIO pins — no hardware peripheral conflict

## Consequences
- Adafruit SSD1306 and Adafruit GFX libraries removed from all platformio.ini
- U8g2 added as replacement
- OledDisplay class internal implementation rewritten — public API unchanged
  (begin(), showStatus(), showError() signatures identical)
- GPIO3=SDA, GPIO10=SCL confirmed working for all MCUs
- Future I2C sensors (RTC, etc.) can share the software I2C bus with OLEDs
  since they have different addresses
