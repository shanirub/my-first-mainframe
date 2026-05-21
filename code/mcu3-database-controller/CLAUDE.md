# MCU #3 — Database Controller

## Identity
- I2C address on shared bus: 0x0A
- Role: account storage proxy — receives DB_READ/DB_WRITE on shared bus,
  forwards to Raspberry Pi 3B+ over UART, returns result on shared bus
- Board: ESP32-WROOM-32 DevKit (38-pin, CP2102, Type-C)
- upload_port = /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0
- monitor_port = /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0

## Architecture Note
MCU #3 does not store data directly. It is the shared bus participant at
0x0A and acts as a proxy between the bus and a Raspberry Pi 3B+ that owns
SQLite storage and the Flask web interface. Communication to RPi is over
UART1 (GPIO18/19) using newline-terminated JSON.

For full architecture reasoning and hardware debugging history, see:
- ADR-008: three board replacements, SD card exhaustion
- ADR-009: UART/RPi decision, protocol, RPi setup requirements
- ADR-010: root cause of Arduino I2C slave failures; decision to switch to
  pure ESP-IDF (framework = espidf)
- ADR-011: (to be written) pure ESP-IDF implementation decisions

For RPi setup steps, Python script design, and SQLite schema, see:
- code/raspi-db-server/CLAUDE.md

For implementation status and next steps, see the project roadmap:
- docs/roadmap.md — MCU #3 phases 0–4 with checkboxes

## Platform
MCU #3 uses **pure ESP-IDF** via `framework = espidf` on the official
PlatformIO `espressif32` platform, pinned to v6.10.0 (IDF 5.4).

Rationale: arduino-esp32 3.x auto-initializes Wire (I2C_NUM_0) on GPIO8/9
before `setup()` runs, poisoning the GPIO matrix for `i2c_new_slave_device()`.
All Arduino-layer workarounds failed. Pure ESP-IDF eliminates the conflict
at the root — no framework touches GPIO8/9 before our code runs.
See ADR-010 for full investigation history.

```ini
platform = espressif32 @ 6.10.0
framework = espidf
board = esp32dev
```

IDF version pinning rationale:
- espressif32 @ 6.10.0 → IDF 5.4 — confirmed stable, `i2c_new_slave_device()`
  available (requires IDF ≥ 5.2), avoids IDF 6.0 unknowns
- espressif32 (unpinned) as of May 2026 resolves to v7.0.0 → IDF 6.0 —
  our slave API survives (v1 was removed, v2 which we use is retained),
  but IDF 6.0 has other breaking changes; defer upgrade until stable
- Do NOT use pioarduino for MCU #3 — no longer needed with framework = espidf

Other MCUs (#1, #2, #4, #5) remain on espressif32@7.0.1 with framework = arduino.
MCU #3 is the only board on espidf.

## Monitor Baud Rate
**921600** — matches the XTAL-clock baud rate requirement.
Under ESP-IDF, `uart_driver_install()` on UART0 (debug output) must be
configured at 921600. This is the same constraint as under pioarduino —
the XTAL/APB clock source issue is IDF-level, not Arduino-level.
`monitor_speed = 921600` in platformio.ini.

## Hardware History
MCU #3 went through three board replacements before reaching this board.
See ADR-008. Short version:
1. ESP32-C3 SuperMini — insufficient free GPIOs
2. WeMos LOLIN32 Lite clone — GPIO8/9 tied to 32kHz crystal
3. ESP32-WROOM-32 DevKit (current) — GPIO8/9 available, all pins clean

## Pin Configuration
| Function | GPIO | Notes |
|---|---|---|
| Shared bus SDA | GPIO8 | I2C_NUM_0 slave — hub wiring unchanged |
| Shared bus SCL | GPIO9 | I2C_NUM_0 slave — hub wiring unchanged |
| OLED SDA | GPIO16 | I2C_NUM_1 master — hardware I2C |
| OLED SCL | GPIO17 | I2C_NUM_1 master — hardware I2C |
| UART TX (to RPi) | GPIO18 | UART1 — RPi GPIO15 (RX) |
| UART RX (from RPi) | GPIO19 | UART1 — RPi GPIO14 (TX) |

GPIO5, GPIO23 are unconnected. SD card module is not used.

Both I2C buses are hardware peripherals (not bit-banged):
- I2C_NUM_0 on GPIO8/9: slave role, shared inter-MCU bus
- I2C_NUM_1 on GPIO16/17: master role, private OLED bus

## UART Protocol
Newline-terminated JSON. MCU #3 writes a request and reads until `\n`.
Timeout: 500ms — on expiry, return `Status::TIMEOUT` to MCU #2.

Requests to RPi:
```json
{"op":"read","ac":"12345678"}
{"op":"write","ac":"12345678","nb":60000,"tt":1,"mid":"uuid"}
```

Responses from RPi:
```json
{"bal":50000,"st":0}
{"st":0}
```

## FreeRTOS Task Architecture
Under pure ESP-IDF, `app_main()` replaces Arduino `setup()`/`loop()`.
FreeRTOS is started by the IDF before `app_main()` is called — do NOT
call `vTaskStartScheduler()`. `app_main()` creates tasks and returns
(or blocks); it does not loop.

| Task | Priority | Stack | Role |
|---|---|---|---|
| Receiver | 3 | STACK_SIZE_RECEIVER | Calls sharedBus.poll(), puts messages on inboundQueue |
| Logic | 2 | STACK_SIZE_LOGIC | Handles HEARTBEAT→ACK, routes DB_READ/DB_WRITE via queues |
| UART | 2 | STACK_SIZE_LOGIC | Owns UART1 exclusively — sends requests to RPi, reads responses |
| OLED | 1 | STACK_SIZE_OLED | Reads SharedState under displayMutex every 500ms |

## Internal Queues
| Queue | Producer | Consumer | Depth | Purpose |
|---|---|---|---|---|
| inboundQueue | Receiver | Logic | 8 | Decoded bus messages (HEARTBEAT, DB_READ, DB_WRITE) |
| uartQueue | Logic | UART task | 4 | UART operation requests to RPi |
| uartResultQueue | UART task | Logic | 4 | UART operation results from RPi |

## Shared Bus Library
`shared_bus_wroom` — WROOM-specific, pure ESP-IDF implementation.
Lives in code/shared/libs/shared_bus/ alongside shared_bus.h/.cpp.
Same API as `shared_bus` (used by ESP32-C3 MCUs): init(), send(), poll().
Selected at compile time via `#ifdef MCU_BOARD_WROOM`.

Written from scratch for IDF 5.x — no Arduino dependencies.
Internally uses `i2c_new_slave_device()` (IDF 5.2+, slave driver v2,
activated via `CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2=y`).
Push-based receive via ISR callback → FreeRTOS semaphore → poll() unblocks.
Non-blocking transmit via `i2c_slave_write()`.

**Naming convention for shared/libs:**
Files without "wroom" in their name target ESP32-C3 SuperMini (Arduino).
Files with "wroom" in their name target ESP32-WROOM-32 DevKit (MCU #3, ESP-IDF).

## OLED Library
`oled_display_wroom` — WROOM-specific, pure ESP-IDF implementation.
Uses U8g2 in HAL-callback mode (no Arduino layer):
- `i2c_new_master_bus()` + `i2c_master_transmit()` for I2C_NUM_1
- Two callbacks provided to U8g2: `u8x8_byte_i2c_cb` and `u8x8_gpio_delay_cb`
- No `u8g2-hal-esp-idf` community library — written from scratch using
  the new IDF master API (avoids deprecated `i2c_driver_install()`)

## Build System
Under `framework = espidf`, PlatformIO uses CMake (the native IDF build system).
Required files (not generated — must be created and maintained):
- `CMakeLists.txt` (project root): `cmake_minimum_required`, IDF include, `project()`
- `main/CMakeLists.txt`: lists source files via `idf_component_register()`
- `sdkconfig.defaults`: hand-maintained overrides (committed to VCS)
  - `CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2=y`
  - `CONFIG_ESP_MAIN_TASK_STACK_SIZE=4096` (or larger if needed)
- `sdkconfig`: generated by IDF from sdkconfig.defaults + defaults — do NOT commit

The `managed_components/` directory and `dependencies.lock` are generated
by the IDF Component Manager and should not be committed.

## Logic Flow
```
app_main() initializes peripherals, creates tasks, returns.

Receiver calls sharedBus.poll() → blocks on semaphore → ISR fires on message → puts on inboundQueue

Logic wakes on inboundQueue:

  HEARTBEAT from MCU #1:
  → sharedBus.send(ACK)
  → update SharedState

  DB_READ from MCU #2:
  → put UartRequest{op=read, account} on uartQueue
  → block on uartResultQueue (500ms timeout)
  → on timeout:  sharedBus.send(DB_READ_RESULT, Status::TIMEOUT)
  → on result:   sharedBus.send(DB_READ_RESULT, balance)
  → update SharedState

  DB_WRITE from MCU #2:
  → put UartRequest{op=write, account, newBalance, txnType, mid} on uartQueue
  → block on uartResultQueue (500ms timeout)
  → on timeout:  sharedBus.send(DB_WRITE_ACK, Status::TIMEOUT)
  → on result:   sharedBus.send(DB_WRITE_ACK, Status::OK)
  → update SharedState

UART task wakes on uartQueue:
  → serialize request to JSON
  → uart_write_bytes(UART_NUM_1, json)
  → uart_read_bytes() with 500ms timeout
  → put UartResult on uartResultQueue

OLED task wakes every 500ms:
  → take displayMutex
  → read SharedState
  → render to display
  → give displayMutex
```

## SharedState
```cpp
struct SharedState {
    uint32_t readCount;
    uint32_t writeCount;
    char     lastAccount[9];
    char     lastError[24];
};
```

## OLED Layout
```
DATABASE CTRL
Addr: 0x0A
R:128 W:64
Last: 12345678
```

## Critical Notes
- Do NOT call vTaskStartScheduler() — IDF starts FreeRTOS before app_main()
- app_main() should create tasks then return (or block indefinitely) —
  there is no loop() equivalent; the loopTask does not exist under espidf
- sharedBus.init() and oled.init() must be called before xTaskCreate()
- UART1 (GPIO18/19) is for RPi — UART0 (USB/CP2102) is for debug logging only
- RPi must be booted before MCU #3 starts — UART timeout handles RPi-down gracefully
- Disconnect shared bus wires before flashing — bus activity during flash corrupts firmware
- GPIO8/9 availability must be verified on any future board replacement —
  these are crystal pins on some boards (LOLIN32 Lite) and unavailable
- Under espidf, ESP_LOGI/ESP_LOGW replace Serial.print for debug output
- monitor_speed = 921600 — do not change
- IDF 6.0 (espressif32 @ 7.0.0) removes I2C slave v1 but retains v2 (our API).
  Upgrading from 6.10.0 to 7.0.0 requires review of other IDF 6.0 breaking changes
  before proceeding — do not upgrade without a dedicated test session