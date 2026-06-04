# Mainframe Simulation — Claude Context

## Project Summary
Educational simulation of a banking mainframe using four ESP32-C3 microcontrollers and a
Raspberry Pi 3B+, each representing a distinct subsystem communicating over a shared I2C
bus. Built on Fedora Linux with VS Code + PlatformIO (MCUs) and Python 3 (RPi).

## Hardware
- MCUs: 4× ESP32-C3 SuperMini (#1, #2, #4, #5)
- Database Controller: Raspberry Pi 3B+ (replaces MCU #3 — see ADR-011)
- Displays: 0.96" SSD1306 128×64 OLED, one per device (5 total)
- Breadboards: 2× long (64-row), 3× short (30-row) + 1× large for RPi area, T-shape on 30×30cm wood base
- Logic analyzer: 8-channel 24MHz (PulseView/Sigrok)
- Bench supply: Wanptek WPS3010H (5V, 1A confirmed working for all MCUs)
- RPi power: dedicated 5V/3A USB supply — do NOT share with bench supply

> ESP32-WROOM-32 DevKit (38-pin, CP2102) — retired as MCU #3. See ADR-011.

## Physical Layout
T-shape on 30×30cm wood base:
- Vertical long BB (spine): RPi 3B+ on separate large BB + shared bus hub (bottom)
- Horizontal long BB (base): MCU #4 (left) + MCU #5 (right)
- Short BB left: MCU #1 · Short BB right: MCU #2
- Hub has SDA/SCL/GND rails + two 5kΩ pull-ups (one per signal)
- All breadboards share common GND via daisy chain black wires

## I2C Bus Architecture — Per MCU
Each ESP32-C3 MCU runs two I2C buses:
- Hardware TwoWire(0): GPIO8=SDA, GPIO9=SCL, 400kHz — shared inter-device bus
- U8g2 software I2C: pins vary per board — private OLED display

ESP32-C3 has only one hardware I2C peripheral (SOC_I2C_NUM=1).
TwoWire(1) silently fails. U8g2 SW_I2C avoids the conflict entirely.

## OLED Pin Configuration
OLED pins are per-device (defined in each MCU's config.h, not shared_config.h):
- MCUs #1, #2, #4, #5 (ESP32-C3 SuperMini): GPIO3=SDA, GPIO10=SCL
- RPi 3B+: GPIO2=SDA, GPIO3=SCL (BSC1 master, luma.oled, /dev/i2c-1)

## MCU Addressing
| MCU | Role | Board | I2C Address | USB port |
|-----|------|-------|-------------|----------|
| #1 | Master Console | ESP32-C3 SuperMini | 0x08 | usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B1:F1:74-if00 |
| #2 | Transaction Processor | ESP32-C3 SuperMini | 0x09 | usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B0:C9:CC-if00 |
| #4 | Job Scheduler | ESP32-C3 SuperMini | 0x0B | usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:AF:AB:94-if00 |
| #5 | I/O Controller | ESP32-C3 SuperMini | 0x0C | usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:AF:5F:A4-if00 |

> MCU #3 (ESP32-WROOM-32 DevKit) retired — see ADR-011.

---

## Raspberry Pi 3B+ — Database Controller

**I2C address:** 0x0A (same address previously held by MCU #3 — no changes to other
devices required)

**Role:** Database Controller. Receives DB_READ, DB_WRITE, and HEARTBEAT from the shared
bus. Returns results via master-read (RPi is a pure slave — it never initiates). Owns
SQLite storage (Phase 4+) and Flask web interface (Phase 4+).

**Connection:** SSH — raspi-db-server (Tailscale)

**Shared bus pins:**
- GPIO18 = SDA, GPIO19 = SCL
- BCM2837 BSC slave peripheral (`pigpio bsc_i2c()`)
- Hardware-backed I2C slave — GPIO18/19 are hardwired to this peripheral in silicon
- Connects to shared bus hub alongside the MCUs

**OLED pins:**
- GPIO2 = SDA, GPIO3 = SCL
- BCM2837 BSC1 master peripheral (`luma.oled`, `/dev/i2c-1`)
- Private wire to SSD1306 — does NOT connect to shared bus hub
- Separate hardware from GPIO18/19 — no interference

**Power:** Dedicated 5V/3A USB supply. Do not power from bench supply.

**Software:** Python 3 service. Three threads: `bus_worker`, `oled_worker`,
`flask` (Phase 4+). See `docs/design/raspi_architecture.md` for full thread design
and `code/raspi-db-server/CLAUDE.md` for setup steps and DoD checklists.

---

## Shared Libraries
All in code/shared/libs/ — picked up automatically via lib_extra_dirs = ../shared/libs

| Library | Purpose |
|---------|---------|
| oled_display | U8g2 SW_I2C wrapper for ESP32-C3 (Arduino): begin(), showStatus(), showError() |
| shared_bus | TwoWire(0) abstraction: init(), send(), poll(), sendAndReceive() — FreeRTOS task-safe |
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
- `sendAndReceive(uint8_t target, const char* txMsg, char* rxBuf, uint32_t timeoutMs=500)`
  — sends a message to target, then polls `requestFrom(target, 256)` until byte[0]
  (the ready flag) is `0x01` or timeoutMs elapses. Reads exactly 256 bytes into rxBuf.
  Returns Status::OK or Status::TIMEOUT. busMutex is held across the full send + poll
  sequence. Required for all RPi interactions (MCU #1 HEARTBEAT, MCU #2 DB_READ/DB_WRITE).
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
- SD.h file paths require leading slash: "/accounts.json" not "accounts.json" [historical — SD card design retired]
- FAT32 volumes over 32GB are unreliable with Arduino SD.h — use ≤8GB partition [historical — SD card design retired]
- arduino-esp32 3.x auto-initializes Wire on GPIO8/9 before setup() — blocks i2c_new_slave_device(). Fix: framework = espidf (ADR-010)
- u8g2-hal-esp-idf hardcodes I2C bus speed to 50kHz and uses legacy i2c driver (driver/i2c.h) on I2C_NUM_1 — safe in IDF 5.4.0 as long as no other code uses i2c_new_master_bus() on I2C_NUM_1
- u8g2 I2C address must be left-shifted by 1: 0x3C → 0x78 in u8x8_SetI2CAddress()
- RPi BSC slave TX FIFO is cleared by hardware after the MCU reads it — clear_tx_fifo()
  (loading NOT_READY + zeros) is called at startup and between transactions to prevent
  stale responses from a previous transaction being read by the next request
- pigpiod must be running before the Python service starts — add to /etc/rc.local or
  systemd. If pigpiod is not running, bsc_i2c() calls will fail silently or raise OSError.

## Current Architecture Status
Phase 3 in progress (2026-06-03).

**What works:**
- All 4 MCUs physically connected and wired to shared bus hub
- FreeRTOS task pattern running on all 4 MCUs
- SharedBus v2: mutex + rxSemaphore + runtime mode switching confirmed stable
- MCU #1: heartbeat to all slaves, timestamp-based ACK tracking, SharedState pattern
- MCUs #2, #4, #5: receiving heartbeats and sending ACKs
- receiverTask extracted to shared library (ReceiverParams pattern)
- RPi 3B+ acquired, identified as raspi-db-server on Tailscale network

**What needs doing next:**
- RPi Phase 3.1 — pigpio BSC slave init on GPIO18/19, confirm bus participation
- RPi Phase 3.2 — OLED smoke test (static text on screen via luma.oled)
- RPi Phase 3.3 — HEARTBEAT handling + stub HEARTBEAT_ACK response
- SharedBus: implement sendAndReceive() (required for Phase 3 HEARTBEAT from MCU #1)
- MCUs #2, #4, #5: subsystem logic (see roadmap.md for details)

## Key Design Decisions for Phase 3
- MCU #2: sequential transaction handling (one at a time) — state machine deferred to Phase 4
- MCU #4: immediate job dispatch — priority queue deferred to Phase 4
- MCU #5: single pending request slot — expand to 4 in Phase 4
- MCU #1: serial monitor commands for operator input in Phase 3, web dashboard in Phase 4
- Serial command format: `DEPOSIT 12345678 10000` (amounts in cents)
- RPi Phase 3: stub responses only — SQLite deferred to Phase 4

## PulseView Setup
- D0 = SDA (GPIO8, orange wire)
- D1 = SCL (GPIO9, white/grey wire)
- In decoder: assign SCL→D1, SDA→D0 (opposite of channel defaults)

## Git Hook
scripts/claude_memory_sync.py — post-commit, fires on CLAUDE.md changes.
Summarization works. Memory write pending proper context-management API implementation.

## Preferences
- Never assume — always ask if a fact is missing
- Verify all API symbols via mcp-api-doc before writing code
- State assumptions explicitly, flag uncertainty
- Architecture decisions: present at least two options with reasoning
- Debugging: list plausible causes with probability, propose targeted tests
- This is a learning project — use professional terms with brief inline explanations
- Never use a value, address, or path from a previous test run as an example
  in a new instruction without explicitly stating which run it came from
- Never assume a file path exists — if a path is inferred or guessed, say so
  explicitly before asking the user to run a command against it
- Never state a hypothesis mid-response and retract it in the same response —
  form the complete thought before stating it
- Never move to a new investigation step without first explicitly stating what
  conclusion the previous step reached and why the next step follows from it
- When a config change is made, always verify it took effect by checking the
  compile timestamp in the boot log before interpreting new results