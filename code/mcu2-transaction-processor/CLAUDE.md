# MCU #2 — Transaction Processor

## Identity
- I2C address on shared bus: 0x09
- Role: validates and routes banking transactions

## Current State
- OLED working (confirmed): GPIO3=SDA, GPIO10=SCL
- Shared bus I2C: GPIO8=SDA, GPIO9=SCL (not yet tested)
- main_oled_backup.cpp: working OLED code, preserved before Task 3
- main.cpp: currently I2C slave test code (Task 3)

## Task 3 — Slave Code
- Acts as I2C slave at 0x09 on shared bus
- onReceive() prints received bytes to serial
- No OLED active during this task

## After Task 3
Restore OLED by merging main_oled_backup.cpp back in:
- Keep TwoWire(0) for shared bus (GPIO8/9)
- Keep TwoWire(1) or existing OledDisplay for OLED (GPIO3/10)
- Replace raw #define SDA_PIN 3 with OLED_SDA_PIN from config.h