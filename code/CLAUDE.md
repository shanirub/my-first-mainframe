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
| shared_bus | TwoWire(0) abstraction: init(), send(), poll() — FreeRTOS task-safe |
| message_protocol | JSON envelope builder, schema validation, constants |

shared/config/shared_config.h — all pin definitions, MCU addresses, and
FreeRTOS stack size constants. Included via build_flags = -I ../shared/config.

## SharedBus API (current — FreeRTOS)
- `init(uint8_t address)` — replaces beginMaster()/beginSlave(). Creates
  busMutex and rxSemaphore, initialises TwoWire(0) in slave mode, registers ISR.
  Must be called before xTaskCreate().
- `send(uint8_t target, const char* msg)` — takes busMutex, switches to master,
  transmits, switches back to slave, gives mutex. Safe from any task.
- `poll(char* buf, int len)` — blocks calling task on rxSemaphore until ISR
  signals a message arrived. Call from receiver task only.
- `busMutex` — public SemaphoreHandle_t, exposed if tasks need direct access.

## FreeRTOS Task Pattern
Every MCU uses: Receiver (pri=3), Logic (pri=2), OLED (pri=1).
Subsystem-specific tasks added per MCU role (see freertos_architecture.md).
Display state protected by dedicated displayMutex (separate from busMutex).
All handles are globals in main.cpp.

Do NOT call vTaskStartScheduler() — ESP32 Arduino starts FreeRTOS before
setup(). Call vTaskDelete(NULL) in loop() to reclaim loopTask stack.

## Stack Size Constants (shared_config.h)
Units are bytes (ESP32 xTaskCreate takes bytes, not words):
- STACK_SIZE_SENDER / STACK_SIZE_LOGIC = 4096 (JSON + Serial tasks)
- STACK_SIZE_RECEIVER / STACK_SIZE_OLED = 2048 (shallow tasks)
- HTTP server task uses 8192 (WiFi stack requirement)

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
  because ESP32 Arduino already started FreeRTOS before setup() ran.

## Current Architecture Status
Phase 3 in progress (2026-04-15). All 5 MCUs running FreeRTOS task pattern simultaneously.

**What works:**
- All 5 MCUs on shared bus: receiver/logic/OLED tasks running, heartbeats ACKed
- MCU #1: heartbeat task (all 4 slaves, 10s interval), timestamp-based health tracking
  on OLED (Act:X/4 via lastAckTime + HEARTBEAT_ACK_TIMEOUT_MS=30s)
- SharedBus v2: mutex + rxSemaphore + runtime mode switching confirmed stable at 5 MCUs
- MessageProtocol library: unchanged, fully compatible
- OledDisplay library: unchanged

**What needs doing (Phase 3 remainder):**
- MCU #1: serial console commands — DEPOSIT/WITHDRAW/BALANCE dispatch to MCU #4
  (serialInputTask + logicTask stub exist; dispatch logic not yet implemented)
- MCU #2: sequential transaction handling — one transaction at a time
- MCU #3: SD card read/write (accounts.json + transactions.log)
- MCU #4: immediate job dispatch — receive JOB_SUBMIT, forward to MCU #2
- MCU #5: WiFi/HTTP server, web console, I2C logic task
- End-to-end transaction flow: DEPOSIT, WITHDRAW, BALANCE

## Key Design Decisions for Phase 3
- MCU #2: sequential transaction handling (one at a time) — state machine deferred to Phase 4
- MCU #4: immediate job dispatch — priority queue deferred to Phase 4
- MCU #5: single pending request slot — expand to 4 in Phase 4
- MCU #1: serial monitor commands for operator input in Phase 3, web dashboard in Phase 4
- Serial command format: `DEPOSIT 12345678 10000` (amounts in cents)

## Next Steps (Phase 3 continuation)
1. MCU #1: implement serial command dispatch (DEPOSIT/WITHDRAW/BALANCE → MCU #4)
2. MCU #4: implement job dispatch (receive JOB_SUBMIT from MCU #1, forward to MCU #2)
3. MCU #2: implement sequential transaction handling
4. MCU #3: implement SD card read/write for account data
5. MCU #5: implement WiFi/HTTP server and I2C logic task
6. End-to-end flow test: DEPOSIT/WITHDRAW/BALANCE through full chain

## PulseView Setup
- D0 = SDA (GPIO8, orange wire)
- D1 = SCL (GPIO9, white/grey wire)
- In decoder: assign SCL→D1, SDA→D0 (opposite of channel defaults)

## Git Hook
scripts/claude_memory_sync.py — post-commit, fires on CLAUDE.md changes.
Summarization works. Memory write pending proper context-management API implementation.
