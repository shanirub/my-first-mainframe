# MCU #1 — Master Console

## Identity
- I2C address on shared bus: 0x08
- Role: system coordinator, operator interface, command input
- upload_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B1:F1:74-if00
- monitor_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B1:F1:74-if00

## Current State
- OLED working (confirmed): GPIO3=SDA, GPIO10=SCL
- Shared bus I2C: GPIO8=SDA, GPIO9=SCL CONFIRMED WORKING
- main_oled_backup.cpp: working OLED code, preserved
- main.cpp: I2C master test code (Task 3 complete)
- Unique USB port ID: usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B1:F1:74-if00

## Next
Merge OLED code back in from main_oled_backup.cpp:
- Keep TwoWire(0) for shared bus (GPIO8/9)
- Add TwoWire(1) for OLED bus (GPIO3/10)
- Replace raw #define SDA_PIN 3 with OLED_SDA_PIN from config.h

## Planned Features (later phases)
- Serial console: accept commands via USB serial input
- WiFi web dashboard: HTTP server on port 80
- Heartbeat: poll all subsystems and display active count on OLED
- Transaction logging: log all inter-subsystem messages