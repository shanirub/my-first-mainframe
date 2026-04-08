# Mainframe Simulation — Claude Context

## Project Summary
Educational simulation of a banking mainframe using 5× ESP32-C3 SuperMini
MCUs. Each MCU represents a distinct subsystem communicating over a shared
I2C bus. Built on Fedora Linux with VS Code + PlatformIO.

## Hardware
- MCUs: 5× ESP32-C3 SuperMini
- Displays: 0.96" SSD1306 128×64 OLED, one per MCU
- Breadboards: 2× long (64-row), 3× short (30-row), T-shape on 30×30cm wood base
- Logic analyzer: 8-channel 24MHz (PulseView/Sigrok)
- Bench supply: Wanptek WPS3010H (5V, 1A confirmed working for all 5 MCUs)

## Physical Layout
T-shape on 30×30cm wood base:
- Vertical long BB (spine): MCU #3 (top) + shared bus hub (bottom)
- Horizontal long BB (base): MCU #4 (left) + MCU #5 (right)
- Short BB left: MCU #1 · Short BB right: MCU #2
- Hub has SDA/SCL/GND rails + two 5kΩ pull-ups (one per signal)

## I2C Bus Architecture — Per MCU
Each MCU runs two I2C buses:
- Hardware TwoWire(0): GPIO8=SDA, GPIO9=SCL, 400kHz — shared inter-MCU bus
- U8g2 software I2C:   GPIO3=SDA, GPIO10=SCL — private OLED display

ESP32-C3 has only one hardware I2C peripheral (SOC_I2C_NUM=1).
TwoWire(1) silently fails. U8g2 SW_I2C avoids the conflict entirely.

## MCU Addressing
| MCU | Role                  | I2C Address | USB port (by-id)                                              |
|-----|-----------------------|-------------|---------------------------------------------------------------|
| #1  | Master Console        | 0x08        | usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B1:F1:74-if00 |
| #2  | Transaction Processor | 0x09        | usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B0:C9:CC-if00 |
| #3  | Database Controller   | 0x0A        | usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:AF:41:D4-if00 |
| #4  | Job Scheduler         | 0x0B        | usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:AF:AB:94-if00 |
| #5  | I/O Controller        | 0x0C        | usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:AF:5F:A4-if00 |

## Shared Libraries
All in code/shared/libs/ — picked up automatically via lib_extra_dirs = ../shared/libs

| Library | Purpose |
|---------|---------|
| oled_display | U8g2 SW_I2C wrapper: begin(), showStatus(), showError() |
| shared_bus | TwoWire(0) abstraction: beginMaster(), beginSlave(), send(), poll() |
| message_protocol | JSON envelope builder, schema validation, constants |

shared/config/shared_config.h — all pin definitions and MCU addresses.
Included via build_flags = -I ../shared/config (not lib_extra_dirs).

## Known Bugs / Confirmed Fixes
- ESP32-C3 SOC_I2C_NUM=1: only one hardware I2C. TwoWire(1) silently fails.
- TwoWire slave begin() needs 4 args: begin(addr, sda, scl, 0) — frequency mandatory.
- board_build.mcu = esp32c3 mandatory alongside board = esp32-c3-devkitm-1.
- ArduinoJson v7: createNestedObject() deprecated → doc[key].to<JsonObject>()
- ArduinoJson v7: containsKey() deprecated → obj[key].is<JsonVariant>()
- ArduinoJson v7: doc.as<JsonObject>() invalid on const → use JsonObjectConst
- SharedBus _rxBuf must be 256 bytes — default 32 caused IncompleteInput errors
- I2C_BUFFER_LENGTH=256 required in all platformio.ini build_flags

## Current Architecture Status
Phase 2 complete. Phase 2.5 (FreeRTOS pivot) in planning.

**What works:**
- All 5 MCUs receiving heartbeats from MCU #1 (sequential pair tests confirmed)
- OLEDs running simultaneously with shared bus on all MCUs
- MessageProtocol library: UUID v4 job IDs, schema validation, integer cents
- SharedBus library: ISR/poll() pattern, BusError typed returns

**Why FreeRTOS:**
The Arduino loop() model cannot support authentic mainframe subsystem-to-subsystem
communication. TwoWire on ESP32-C3 is master OR slave per boot — a slave cannot
initiate transmissions. This means MCU #4 (JES) cannot send directly to MCU #3
(DASD) without routing through MCU #1. FreeRTOS tasks with mutex-protected bus
access solves this cleanly. See ADR-007.

**What stays the same after FreeRTOS migration:**
- All 5 MCU roles and I2C addresses — unchanged
- OledDisplay library — unchanged
- MessageProtocol library — unchanged
- shared_config.h — unchanged
- Physical wiring and hub — unchanged

**What changes:**
- SharedBus: needs beginMaster()/beginSlave() switching + mutex protection
- Each MCU main.cpp: loop() replaced with FreeRTOS task creation + scheduler start
- Communication pattern: any MCU can initiate, mutex serialises bus access

## Next Steps (Phase 2.5)
1. Validate FreeRTOS multi-master I2C proof of concept on 2 MCUs
2. Redesign SharedBus for task-safe operation
3. Redesign each MCU as a set of FreeRTOS tasks
4. Full 5-MCU simultaneous bus test

## PulseView Setup
- D0 = SDA (GPIO8, orange wire)
- D1 = SCL (GPIO9, white/grey wire)
- In decoder: assign SCL→D1, SDA→D0 (opposite of channel defaults)

## Git Hook
scripts/claude_memory_sync.py — post-commit, fires on CLAUDE.md changes.
Summarization works. Memory write pending proper context-management API implementation.
