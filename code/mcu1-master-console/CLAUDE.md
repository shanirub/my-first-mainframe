# MCU #1 — Master Console

## Identity
- I2C address on shared bus: 0x08
- Role: system coordinator, operator interface, command input
- upload_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B1:F1:74-if00
- monitor_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B1:F1:74-if00

## Current State
- OLED working (confirmed): GPIO3=SDA, GPIO10=SCL via U8g2 software I2C
- Shared bus I2C: GPIO8=SDA, GPIO9=SCL CONFIRMED WORKING
- Both buses running simultaneously: CONFIRMED WORKING
- SharedBus class in use: sharedBus.beginMaster(), sharedBus.send()
- BusError return type used for all send() results
- main.cpp: uses SharedBus + OledDisplay, no raw TwoWire or pin numbers
- main_oled_display.cpp.old: pre-merge backup, preserved
- Phase 1.5 COMPLETE

## Next
Phase 2 — JSON messaging protocol across all 5 MCUs

## Planned Features (later phases)
- Serial console: accept commands via USB serial input
- WiFi web dashboard: HTTP server on port 80
- Heartbeat: poll all subsystems and display active count on OLED
- Transaction logging: log all inter-subsystem messages