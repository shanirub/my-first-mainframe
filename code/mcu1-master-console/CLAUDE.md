# MCU #1 — Master Console

## Identity
- I2C address on shared bus: 0x08
- Role: system coordinator, operator interface, command input

## Current State
- OLED working (confirmed): GPIO3=SDA, GPIO10=SCL
- Shared bus I2C: GPIO8=SDA, GPIO9=SCL (not yet tested)
- main.cpp: OLED working code, showing "MASTER CONSOLE" / "READY"

## After Task 3
- Will become I2C master on shared bus
- Sends messages to MCU #2 (0x09) and other subsystems
- Replace raw #define SDA_PIN 3 with OLED_SDA_PIN from config.h
- Add TwoWire sharedBus = TwoWire(0) for shared bus alongside existing OLED bus

## Planned Features (later phases)
- Serial console: accept commands via USB serial input
- WiFi web dashboard: HTTP server on port 80
- Heartbeat: poll all subsystems and display active count on OLED
- Transaction logging: log all inter-subsystem messages