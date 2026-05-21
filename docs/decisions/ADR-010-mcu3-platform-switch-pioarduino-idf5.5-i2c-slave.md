# ADR-010: MCU #3 framework switch to pure ESP-IDF (framework = espidf)

## Status
Accepted — 2026-05-16 (revised 2026-05-21)

## Amends
ADR-009 (OLED pin note — see below)

---

## Context

MCU #3 (ESP32-WROOM-32) needs to operate as an I2C slave at address 0x0A
on the shared inter-MCU bus (GPIO8/9). All previous attempts to initialize
I2C slave mode failed with either a TG1WDT (Timer Group 1 Watchdog) reset
or a silent GPIO matrix conflict.

### Investigation — what was tested

**Attempt 1: Arduino `Wire.begin()` in slave mode**
Both `Wire` (bus 0) and `Wire1` (bus 1) were tested. Both caused TG1WDT.

Root cause confirmed by source code (`esp32-hal-i2c-slave.c`): `Wire.begin()`
in slave mode calls `i2cSlaveInit()`, which calls `xTaskCreate(i2c_slave_task,
..., priority=20, ...)`. A task at priority 20 on Core 1 prevents IDLE1 from
being scheduled. TG1WDT monitors each core's idle task and fires if IDLE1
does not receive CPU time within ~1 second. Single-core MCUs (#1/#2/#4/#5,
ESP32-C3) are unaffected — no IDLE1 to starve.

**Attempt 2: IDF 4.4.7 `i2c_driver_install()` in slave mode**
Same result — TG1WDT fired inside the call, before the success print was
reached. Root cause not fully traced (candidate: behavior inside
`i2c_isr_register()` or a 4.4.7-specific bug). Not pursued further —
the outcome was sufficient to disqualify IDF 4.4.7.

**Attempt 3: pioarduino (arduino-esp32 3.x, IDF 5.5.x) with `i2c_new_slave_device()`**
`i2c_new_slave_device()` (IDF 5.2+, slave driver v2, interrupt-driven, no
internal task) eliminates the WDT root cause. However, arduino-esp32 3.x
auto-initializes Wire (I2C_NUM_0) on GPIO8/9 before `setup()` runs, claiming
those pins in the IDF GPIO matrix before our code executes.

Sub-attempts to resolve the GPIO matrix conflict:
1. Change `conf.i2c_port` to I2C_NUM_1 — no effect (conflict is at GPIO
   level, not peripheral number level)
2. Swap init order: `sharedBus.init()` before `oled.begin()` — no effect
   (Wire is initialized by the framework before `setup()` runs)
3. Call `gpio_reset_pin(GPIO_NUM_8/9)` before `sharedBus.init()` — board
   hung, no serial output, OLED blank. `gpio_reset_pin()` conflicts with
   something the framework depends on at that point in boot.

All three workarounds failed. The Wire auto-init happens at the framework
bootstrap level, before any user code can intervene.

### Root cause summary

The Arduino layer (any version) auto-initializes Wire on GPIO8/9 before
user code runs. This is by design in arduino-esp32 and cannot be suppressed
at the user level without modifying the framework itself. Any solution that
retains the Arduino layer is fighting this constraint indefinitely.

### Why `i2c_new_slave_device()` is the correct slave API

IDF 4.4.7 `i2c_driver_install()` in slave mode creates an internal FreeRTOS
task at priority 20 (Wire path, confirmed) and has additional instability
on dual-core ESP32-WROOM (observed but not fully traced). IDF 5.2 introduced
`i2c_new_slave_device()` (slave driver v2): interrupt-driven, no internal task,
push-based receive via ring buffer and ISR callback. This is the correct
design for a slave device in a FreeRTOS system with managed task priorities.

`CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2=y` must be set in `sdkconfig.defaults`
to activate driver v2. In IDF 6.0, slave driver v1 is removed and v2 is the
only path — our choice is forward-compatible.

---

## Decision

Switch MCU #3 to **`framework = espidf`** on the official PlatformIO
`espressif32` platform, pinned to v6.10.0 (IDF 5.4):

```ini
[env:mcu3]
platform = espressif32 @ 6.10.0
framework = espidf
board = esp32dev
monitor_speed = 921600
upload_port = /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0
monitor_port = /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0
```

### Why not pioarduino (previous intermediate decision)

pioarduino (arduino-esp32 3.x, IDF 5.5.x) was considered as a lower-effort
path: retain Arduino APIs, gain IDF 5.5. Rejected because it does not solve
the root cause — the GPIO matrix conflict exists in any Arduino-layer build
regardless of IDF version. pioarduino provides the right IDF version but
the wrong framework layer.

### Why not espressif32 @ 7.0.0 (IDF 6.0)

espressif32 v7.0.0 (IDF 6.0) was released April 2026 and is now the default
when `platform = espressif32` is unpinned. Our slave API (`i2c_new_slave_device()`,
driver v2) survives into IDF 6.0 — slave driver v1 was removed, v2 was not.
However, IDF 6.0 has additional breaking changes (MbedTLS v4, C/C++ standard
upgrades) that have not been evaluated for this project. Pinning to 6.10.0
(IDF 5.4) gives a stable, known-good base. Upgrading to IDF 6.0 is deferred
until a dedicated test session.

### Platform line rationale

`espressif32 @ 6.10.0` with `framework = espidf`:
- PlatformIO downloads vanilla IDF 5.4 toolchain on first build — no manual
  installation required
- All source files are listed in `main/CMakeLists.txt` via `idf_component_register()`
- Configuration overrides go in `sdkconfig.defaults` (committed); `sdkconfig`
  itself is generated and should not be committed
- `managed_components/` and `dependencies.lock` are generated — not committed

### Shared bus library

`shared_bus_wroom` is rewritten from scratch for pure ESP-IDF. No Arduino
dependencies. Same public API as `shared_bus` (ESP32-C3 MCUs): `init()`,
`send()`, `poll()`. Internal implementation:
- `init()`: calls `i2c_new_slave_device()` on I2C_NUM_0 / GPIO8/9, registers
  ISR callback, creates FreeRTOS semaphore
- `poll()`: blocks on semaphore; ISR callback gives semaphore on receive,
  copies bytes from ring buffer to internal buffer
- `send()`: calls `i2c_slave_write()` to pre-load TX buffer

`CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2=y` required in `sdkconfig.defaults`.
Include `driver/i2c_slave.h` — do NOT include `driver/i2c.h` (old driver);
they conflict at link time.

### OLED

`oled_display_wroom` is written from scratch for pure ESP-IDF.
Uses U8g2 in HAL-callback mode:
- I2C master on I2C_NUM_1 / GPIO16/17 via `i2c_new_master_bus()` +
  `i2c_master_transmit()` (new IDF 5.x master API — no deprecated calls)
- Two callbacks provided to U8g2 setup function:
  `u8x8_byte_i2c_cb` (sends bytes over I2C_NUM_1)
  `u8x8_gpio_delay_cb` (handles reset pin and delays via `vTaskDelay`)
- No community HAL library (all existing ones use deprecated `i2c_driver_install()`)

### Debug output

`ESP_LOGI` / `ESP_LOGW` / `ESP_LOGE` (IDF logging macros) replace
`Serial.print`. Output appears on UART0 (USB/CP2102) at 921600 baud.
UART1 (GPIO18/19) is reserved exclusively for RPi communication.

---

## Impact on MCU #3

- `platformio.ini`: platform = espressif32 @ 6.10.0, framework = espidf
- `CMakeLists.txt` (root + main/): required, created from scratch
- `sdkconfig.defaults`: `CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2=y` minimum
- `shared_bus_wroom.h/.cpp`: rewritten, no Arduino dependencies
- `oled_display_wroom.h/.cpp`: rewritten, U8g2 HAL-callback mode, new IDF master API
- `main.cpp`: `app_main()` replaces `setup()`/`loop()`; no `vTaskStartScheduler()`

## Impact on other MCUs

None. Framework change is isolated to MCU #3's `platformio.ini`.

---

## Consequences

- No Arduino APIs (Serial, Wire, delay) available in MCU #3 build.
  Replacements: `uart_driver_install()`, `i2c_new_*`, `vTaskDelay()`.
- U8g2 HAL must be written once; it is ~60 lines and well-understood.
- `espressif32` is the official Espressif/PlatformIO platform — lower
  maintenance risk than pioarduino.
- The I2C slave v2 API (`i2c_new_slave_device()`) is the forward-compatible
  path in IDF 6.0+ — this decision does not create future upgrade debt.
- IDF 6.0 upgrade path exists (espressif32 @ 7.0.0) but requires a dedicated
  evaluation session before adoption.

---

## Amendment note for ADR-009

ADR-009 pin table lists OLED GPIO16/17 as "U8g2 SW I2C". Under pure ESP-IDF,
OLED is on hardware I2C_NUM_1 (GPIO16/17) using the new IDF master API.
SW I2C (bit-banged) is not used.