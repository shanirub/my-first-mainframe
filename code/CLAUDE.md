# Mainframe Simulation — Claude Context

## Project Summary
Educational simulation of a banking mainframe using 5 microcontrollers, each
representing a distinct subsystem communicating over a shared I2C bus.
Built on Fedora Linux with VS Code + PlatformIO.

## Hardware
- MCUs: 4× ESP32-C3 SuperMini (#1, #2, #4, #5) + 1× ESP32 DevKit (#3, see ADR-008)
- Displays: 0.96" SSD1306 128×64 OLED, one per MCU
- Breadboards: 2× long (64-row), 3× short (30-row) + 1× large for MCU #3, T-shape on 30×30cm wood base
- Logic analyzer: 8-channel 24MHz (PulseView/Sigrok)
- Bench supply: Wanptek WPS3010H (5V, 1A confirmed working for all 5 MCUs)

## Physical Layout
T-shape on 30×30cm wood base:
- Vertical long BB (spine): MCU #3 on separate large BB (ESP32 DevKit) + shared bus hub (bottom)
- Horizontal long BB (base): MCU #4 (left) + MCU #5 (right)
- Short BB left: MCU #1 · Short BB right: MCU #2
- Hub has SDA/SCL/GND rails + two 5kΩ pull-ups (one per signal)
- All breadboards share common GND via daisy chain black wires

## I2C Bus Architecture — Per MCU
Each MCU runs two I2C buses:
- Hardware TwoWire(0): GPIO8=SDA, GPIO9=SCL, 400kHz — shared inter-MCU bus
- U8g2 software I2C: pins vary per board — private OLED display

ESP32-C3 has only one hardware I2C peripheral (SOC_I2C_NUM=1).
TwoWire(1) silently fails. U8g2 SW_I2C avoids the conflict entirely.

## OLED Pin Configuration
OLED pins are now per-MCU (defined in each MCU's config.h, not shared_config.h):
- MCUs #1, #2, #4, #5 (ESP32-C3 SuperMini): GPIO3=SDA, GPIO10=SCL
- MCU #3 (ESP32 DevKit): GPIO16=SDA, GPIO17=SCL

Reason: ESP32 DevKit GPIO3 is RX0 (UART), GPIO10 is flash SPI — both unsafe
for OLED. GPIO16/17 are clean general-purpose pins. See ADR-008.

## MCU Addressing
| MCU | Role | Board | I2C Address | USB port |
|-----|------|-------|-------------|----------|
| #1 | Master Console | ESP32-C3 SuperMini | 0x08 | usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B1:F1:74-if00 |
| #2 | Transaction Processor | ESP32-C3 SuperMini | 0x09 | usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B0:C9:CC-if00 |
| #3 | Database Controller | ESP32 DevKit | 0x0A | /dev/ttyUSB0 (CH340C) |
| #4 | Job Scheduler | ESP32-C3 SuperMini | 0x0B | usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:AF:AB:94-if00 |
| #5 | I/O Controller | ESP32-C3 SuperMini | 0x0C | usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:AF:5F:A4-if00 |

## Shared Libraries
All in code/shared/libs/ — picked up automatically via lib_extra_dirs = ../shared/libs

| Library | Purpose |
|---------|---------|
| oled_display | U8g2 SW_I2C wrapper: begin(), showStatus(), showError() |
| shared_bus | TwoWire(0) abstraction: init(), send(), poll() — FreeRTOS task-safe |
| message_protocol | JSON envelope builder, schema validation, constants |

shared/config/shared_config.h — pins, MCU addresses, stack sizes, heartbeat timing.
Included via build_flags = -I ../shared/config.

## SharedBus API (current — FreeRTOS)
- `init(uint8_t address)` — creates busMutex and rxSemaphore, initialises TwoWire(0)
  in slave mode, registers ISR. Must be called before xTaskCreate().
- `send(uint8_t target, const char* msg)` — takes busMutex, switches to master,
  transmits, switches back to slave, gives mutex. Safe from any task.
- `poll(char* buf, int len)` — blocks calling task on rxSemaphore until ISR
  signals a message arrived. Call from receiver task only.
- `busMutex` — public SemaphoreHandle_t, exposed if tasks need direct access.
- `receiverTask(void* params)` — shared task function, extracted into shared_bus library.
  Pass a static ReceiverParams* via xTaskCreate. ReceiverParams must be file-scope static.

## FreeRTOS Task Pattern
Every MCU uses: Receiver (pri=3), Logic (pri=2), OLED (pri=1).
Subsystem-specific tasks added per MCU role (see freertos_architecture.md).
SharedState struct + displayMutex pattern for OLED. All handles are globals in main.cpp.

Do NOT call vTaskStartScheduler() — ESP32 Arduino starts FreeRTOS before
setup(). Call vTaskDelete(NULL) in loop() to reclaim loopTask stack.

## Stack Size Constants (shared_config.h)
Units are bytes (ESP32 xTaskCreate takes bytes, not words):
- STACK_SIZE_SENDER / STACK_SIZE_LOGIC = 4096 (JSON + Serial tasks)
- STACK_SIZE_RECEIVER / STACK_SIZE_OLED = 2048 (shallow tasks)
- HTTP server task uses 8192 (WiFi stack requirement)

## Heartbeat Timing (shared_config.h)
- HEARTBEAT_INTERVAL_MS = 10000 (10 seconds between cycles)
- HEARTBEAT_ACK_TIMEOUT_MS = 30000 (3× interval — slave considered inactive after 30s)

## Known Bugs / Confirmed Fixes
- ESP32-C3 SOC_I2C_NUM=1: only one hardware I2C. TwoWire(1) silently fails.
- TwoWire slave begin() needs 4 args: begin(addr, sda, scl, 0) — frequency mandatory.
- board_build.mcu = esp32c3 mandatory alongside board = esp32-c3-devkitm-1.
- ArduinoJson v7: createNestedObject() deprecated → doc[key].to<JsonObject>()
- ArduinoJson v7: containsKey() deprecated → obj[key].is<JsonVariant>()
- ArduinoJson v7: doc.as<JsonObject>() invalid on const → use JsonObjectConst
- SharedBus _rxBuf must be 256 bytes — default 32 caused IncompleteInput errors
- I2C_BUFFER_LENGTH=256 required in all platformio.ini build_flags
- vTaskStartScheduler() must NOT be called — crashes with ESP_ERR_NOT_FOUND
- ReceiverParams struct passed to receiverTask must be file-scope static in main.cpp —
  stack-allocating it in setup() causes dangling pointer after setup() returns
- Pull-up resistors must NOT be used as physical wire bridges across breadboard halves —
  5kΩ in series on the signal path corrupts I2C signals for MCUs on the far side
- ESP32-C3 SuperMini GPIO6/7 are internal flash SPI pins — unavailable for user SPI
- ESP32-C3 SuperMini GPIO4–7 are JTAG reserved
- SD.h file paths require leading slash: "/accounts.json" not "accounts.json"
- FAT32 volumes over 32GB are unreliable with Arduino SD.h — use ≤8GB partition

## Current Architecture Status
Phase 3 in progress (2026-04-16).

**What works:**
- All 5 MCUs physically connected and wired to shared bus hub
- FreeRTOS task pattern running on all 5 MCUs
- SharedBus v2: mutex + rxSemaphore + runtime mode switching confirmed stable
- MCU #1: heartbeat to all 4 slaves, timestamp-based ACK tracking, SharedState pattern
- MCUs #2, #3, #4, #5: receiving heartbeats and sending ACKs
- receiverTask extracted to shared library (ReceiverParams pattern)
- MCU #3 migrated to ESP32 DevKit — board soldered, wired, recognized on /dev/ttyUSB0

**What needs doing next:**
- MCU #3: flash SD init test on ESP32 DevKit
- MCU #3: migrate FreeRTOS skeleton to new board
- MCU #3: implement SD task and DB_READ/DB_WRITE logic

## Key Design Decisions for Phase 3
- MCU #2: sequential transaction handling (one at a time) — state machine deferred to Phase 4
- MCU #4: immediate job dispatch — priority queue deferred to Phase 4
- MCU #5: single pending request slot — expand to 4 in Phase 4
- MCU #1: serial monitor commands for operator input in Phase 3, web dashboard in Phase 4
- Serial command format: `DEPOSIT 12345678 10000` (amounts in cents)

## PulseView Setup
- D0 = SDA (GPIO8, orange wire)
- D1 = SCL (GPIO9, white/grey wire)
- In decoder: assign SCL→D1, SDA→D0 (opposite of channel defaults)

## Git Hook
scripts/claude_memory_sync.py — post-commit, fires on CLAUDE.md changes.
Summarization works. Memory write pending proper context-management API implementation.