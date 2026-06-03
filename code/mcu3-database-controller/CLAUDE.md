# MCU #3 — Database Controller

## Identity
- I2C address on shared bus: 0x0A
- Role: account storage proxy — receives DB_READ/DB_WRITE on shared bus,
  forwards to Raspberry Pi 3B+ over UART, returns result on shared bus
- Board: ESP32-WROOM-32 DevKit (38-pin, CP2102, Type-C)
- upload_port = /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0
- monitor_port = /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0

> Preferences and debugging rules: see project-level CLAUDE.md ## Preferences

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

For RPi setup steps, Python script design, and SQLite schema, see:
- code/raspi-db-server/CLAUDE.md

For implementation status and next steps, see the project roadmap:
- docs/roadmap.md — MCU #3 phases 0–4 with checkboxes

## Platform
MCU #3 uses **pure ESP-IDF** via `framework = espidf` on the official
PlatformIO `espressif32` platform, pinned to v6.10.0 (IDF 5.4.0).

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
- espressif32 @ 6.10.0 → IDF 5.4.0 — confirmed working
- espressif32 (unpinned) as of May 2026 resolves to v7.0.0 → IDF 6.0 —
  our slave API survives (v1 removed, v2 retained), but IDF 6.0 has other
  breaking changes; defer upgrade until stable
- Do NOT use pioarduino — no longer needed with framework = espidf

Other MCUs (#1, #2, #4, #5) remain on espressif32@7.0.1 with framework = arduino.
MCU #3 is the only board on espidf.

## Monitor Baud Rate
**115200** — IDF 5.4.0 console UART defaults to 115200 and requires
`CONFIG_ESP_CONSOLE_UART_CUSTOM=y` to change. Not worth the complexity.
`monitor_speed = 115200` in platformio.ini.

Note: the 921600 requirement was an arduino-esp32 3.x constraint (XTAL clock
source causing baud deviation at 115200). Under pure ESP-IDF that constraint
does not apply.

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
One-time boot handshake: RPi sends PING, MCU responds PONG. RPi initiates
because it boots slower. After handshake, newline-terminated JSON is used.
Timeout: 500ms — on expiry, return Status::TIMEOUT to MCU #2.

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
call `vTaskStartScheduler()`. `app_main()` creates tasks and returns.

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
Uses u8g2 C API + custom HAL callbacks (NOT u8g2-hal-esp-idf).
Reason: u8g2-hal-esp-idf uses the legacy i2c driver (driver/i2c.h) which
conflicts with driver/i2c_slave.h — IDF aborts at boot if both are linked.

Current implementation:
- Uses driver/i2c_master.h (new driver) on I2C_NUM_1
- i2c_new_master_bus() in begin(), device added in byte_cb on BYTE_INIT
- OLED address is 0x3C — new driver handles R/W bit internally (do NOT shift to 0x78)
- Speed: 400kHz
- Singleton pattern for static callbacks
- No extern "C" guards needed — no C headers included

## Build System
Under `framework = espidf`, PlatformIO uses CMake.
Required files:
- `CMakeLists.txt` (project root): sets EXTRA_COMPONENT_DIRS to src/ and
  ../components (shared repo-level components), includes IDF cmake, declares project name
- `src/CMakeLists.txt`: lists source files via `idf_component_register()`
  with REQUIRES for esp_common, log, freertos, driver, esp_driver_i2c,
  esp_driver_gpio, u8g2. Registers oled_display_wroom.cpp and
  shared_bus_wroom.cpp from shared/libs/.
- `sdkconfig.defaults`: hand-maintained overrides (committed to VCS)
  - `CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2=y`
  - `CONFIG_LOG_DEFAULT_LEVEL_INFO=y`
- `sdkconfig.mcu3`: generated by PlatformIO from sdkconfig.defaults —
  DO NOT commit. Must be deleted and rebuilt whenever sdkconfig.defaults changes.
- `managed_components/`, `dependencies.lock`: generated — do NOT commit

External components (code/components/ — shared across all espidf projects):
- `u8g2` — git clone olikraus/u8g2
- u8g2-hal-esp-idf has been DELETED — conflicts with new i2c slave driver

## IDF Driver Notes (verified from source, IDF 5.4.0)
- When `CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2=y`, the build system
  compiles `i2c_slave_v2.c` and excludes `i2c_slave.c` entirely.
  Confirmed in esp_driver_i2c/CMakeLists.txt.
- `i2c_slave_v2.c` and `i2c_slave.c` use different struct layouts for
  `i2c_slave_dev_t` — the active layout is selected by the config flag
  in `i2c_private.h`. They cannot be mixed.
- `i2c_slave_event_callbacks_t` has two different definitions in
  `driver/i2c_slave.h` depending on the version flag:
  - v1: field is `on_recv_done`
  - v2: fields are `on_request` and `on_receive`
  Using the wrong field name causes a silent compile error or undefined behavior.
- `i2c_slave_write()` signature (v2): `(handle, data, len, write_len_out, timeout_ms)`
  — 5 parameters. timeout_ms=0 is non-blocking.
- The platform-level `s_i2c_platform.mutex` in `i2c_common.c` is a
  newlib `_lock_t` — it is acquired and released within each call to
  `i2c_acquire_bus_handle()`. It is NOT held across calls, so sequential
  master+slave init on different ports cannot deadlock on it.
- `i2c_common_set_pins()` for I2C_NUM_1 (master/OLED) calls
  `gpio_set_level()` on the SDA pin, which can leave GPIO8 reading low
  if called before slave init configures GPIO8. Always init OLED after
  confirming GPIO8/9 state, or init slave before OLED.

## Phase 3.1 — Current Blocker (open as of 2026-06-02)
`i2c_new_slave_device()` triggers TG1WDT_SYS_RESET on every boot.

**Confirmed via isolation tests:**
- Hang is inside `i2c_new_slave_device()` — before/after log confirmed
- Not caused by bus activity — reproduced with MCU #1 unplugged
- Not caused by OLED task — reproduced with xTaskCreate commented out
- Not caused by callback registration — reproduced with
  i2c_slave_register_event_callbacks() commented out
- Not caused by OLED init leaving mutex held — reproduced with entire
  OLED section commented out
- GPIO8/9 both read high (correct idle state) when OLED is not initialized
- GPIO8 reads low when OLED init runs before slave init — side effect of
  i2c_common_set_pins() on I2C_NUM_1; not the root cause of the WDT

**WDT type:** TG1WDT_SYS_RESET — system watchdog, fires when idle task
is starved. Both CPUs at panic_handler at reset time (addr2line confirmed).
Backtrace not yet captured — CONFIG_ESP_SYSTEM_PANIC_PRINT_HALT=y not
taking effect; suspect sdkconfig.mcu3 not being regenerated correctly.

**Root cause:** not yet confirmed. Hang is somewhere inside
`i2c_new_slave_device()` in `i2c_slave_v2.c`. All software-level
hypotheses exhausted from source code reading. Next session should
focus on getting the panic backtrace to identify the exact hang location.

## Logic Flow
```
app_main():
  uart_init()
  uart_handshake()   — blocks until RPi sends PING, MCU sends PONG
  [Phase 2+: oled_init()]
  [Phase 3+: sharedBus.init()]
  xTaskCreate() × 4
  return

Receiver calls sharedBus.poll() → blocks on semaphore → ISR fires on
message → puts on inboundQueue

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
  → uart_read_line() with 500ms timeout
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
- app_main() creates tasks then returns — no loop() equivalent under espidf
- uart_handshake() must complete before tasks are created — RPi must be
  running before MCU #3 is reset
- UART1 (GPIO18/19) is for RPi — UART0 (USB/CP2102) is for debug logging only
- Disconnect shared bus wires before flashing — bus activity during flash corrupts firmware
- GPIO8/9 availability must be verified on any future board replacement —
  these are crystal pins on some boards (LOLIN32 Lite) and unavailable
- Under espidf, ESP_LOGI/ESP_LOGW/ESP_LOGE replace Serial.print
- monitor_speed = 115200 — IDF 5.4.0 console default, no override needed
- IDF 6.0 (espressif32 @ 7.0.0) removes I2C slave v1 but retains v2 (our API).
  Do not upgrade without a dedicated test session
- driver/i2c.h (old driver) must not be included once driver/i2c_slave.h is
  in use — conflict causes build failure
- u8g2-hal-esp-idf must NOT be used alongside the new slave driver —
  deleted from code/components/