# MCU #2 — Transaction Processor

## Identity
- I2C address on shared bus: 0x09
- Role: validates and routes banking transactions
- upload_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B0:C9:CC-if00
- monitor_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B0:C9:CC-if00

## Current State
- OLED working (confirmed): GPIO3=SDA, GPIO10=SCL
- Shared bus I2C: GPIO8=SDA, GPIO9=SCL CONFIRMED WORKING
- main_oled_backup.cpp: working OLED code, preserved
- main.cpp: I2C slave test code (Task 3 complete)
- Unique USB port ID: usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B0:C9:CC-if00

## Critical Fix
sharedBus.begin() for slave mode requires 4 arguments:
begin(I2C_ADDRESS, SHARED_SDA_PIN, SHARED_SCL_PIN, 0)
The frequency argument (0) is mandatory — no default value in slave overload.
Without it, pins are silently ignored and slave never responds.

## After Task 3
Merge main_oled_backup.cpp back in:
- Keep TwoWire(0) for shared bus (GPIO8/9)
- Add TwoWire(1) for OLED bus (GPIO3/10)  
- Replace raw #define SDA_PIN 3 with OLED_SDA_PIN from config.h