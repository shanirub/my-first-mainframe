# MCU #2 — Transaction Processor

## Identity
- I2C address on shared bus: 0x09
- Role: validates and routes banking transactions
- upload_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B0:C9:CC-if00
- monitor_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B0:C9:CC-if00

## Current State
- OLED working (confirmed): GPIO3=SDA, GPIO10=SCL via U8g2 software I2C
- Shared bus I2C: GPIO8=SDA, GPIO9=SCL CONFIRMED WORKING (slave mode)
- Both buses running simultaneously: CONFIRMED WORKING
- SharedBus class in use: sharedBus.beginSlave(I2C_ADDRESS), sharedBus.poll()
- Receive pattern: ISR buffers into _rxBuf, poll() drains safely from loop()
  — no I2C/OLED calls in interrupt context
- main.cpp: uses SharedBus + OledDisplay, no raw TwoWire or pin numbers
- main_oled_backup.cpp.old: pre-merge backup, preserved
- Phase 1.5 COMPLETE

## Critical Fixes (preserved for reference)
- sharedBus.begin() slave overload requires 4 args: begin(addr, sda, scl, 0)
  The frequency argument (0) is mandatory — no default. Without it, slave never responds.
- TwoWire(1) silently fails on ESP32-C3 (SOC_I2C_NUM=1). OLED uses U8g2 SW_I2C.

## Next
Phase 2 — JSON messaging protocol across all 5 MCUs