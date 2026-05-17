# ADR-010: MCU #3 platform switch to pioarduino (IDF 5.5.x) for I2C slave

## Status
Accepted — 2026-05-16

## Amends
ADR-009 (OLED pin note — see below)

---

## Context

MCU #3 (ESP32-WROOM-32) needs to operate as an I2C slave at address 0x0A
on the shared inter-MCU bus (GPIO8/9). All previous attempts to initialize
I2C slave mode failed with a TG1WDT (Timer Group 1 Watchdog) reset.

### Investigation — what was tested

Two separate approaches to I2C slave initialization were tested:

**Attempt 1: Arduino `Wire.begin()` in slave mode**
Both `Wire` (bus 0) and `Wire1` (bus 1) were tested. Both caused TG1WDT.

**Attempt 2: IDF 4.4.7 `i2c_driver_install()` in slave mode (direct call,
bypassing Arduino Wire entirely)**
Same result — TG1WDT fired. The reset occurred during the
`i2c_driver_install()` call itself: the print statement that executes only
on `ESP_OK` return was never reached. The WDT backtrace showed
`_xt_context_save` at PC=0x40087328 (Xtensa context save routine called
during FreeRTOS task switching).

### Root cause — Wire path (confirmed by source code)

The Arduino Wire slave path was traced to source:

File: `framework-arduinoespressif32/cores/esp32/esp32-hal-i2c-slave.c`

`Wire.begin()` in slave mode calls `i2cSlaveInit()` (line 217), which calls:

```c
xTaskCreate(i2c_slave_task, "i2c_slave_task", 4096, i2c, 20, &i2c->task_handle);
```
(line 282)

`i2c_slave_task` (line 807) blocks on `xQueueReceive(..., portMAX_DELAY)` —
it does not busy-wait, but it is created at priority 20, which is higher than
all user tasks and higher than the Arduino `loopTask`.

On the dual-core ESP32-WROOM-32, FreeRTOS pins tasks to cores. A task at
priority 20 running on Core 1 prevents IDLE1 from being scheduled. TG1WDT
monitors each core's idle task independently and fires if IDLE1 does not
receive CPU time within approximately 1 second. This is the confirmed
mechanism for the Wire path WDT.

Single-core MCUs (#1, #2, #4, #5 — ESP32-C3) are not affected: they have
only one core, so there is no IDLE1 to starve.

### Root cause — `i2c_driver_install()` path (observed, not fully explained)

The IDF 4.4.7 `i2c_driver_install()` source was read in full
(`framework-espidf/components/driver/i2c.c`, lines 240–end). In slave mode
it performs: heap allocation, two `xRingbufferCreate` calls, two
`xSemaphoreCreateMutex` calls, ISR registration via `i2c_isr_register()`,
and a slave RX interrupt enable. It creates no FreeRTOS task and contains
no polling loop or busy-wait. Based on source alone, the function should
return in microseconds.

Despite this, the WDT fired inside the call. The cause is not explained by
the source code of `i2c_driver_install()` itself. Candidate explanations
include behavior inside `i2c_isr_register()` or `i2c_hw_enable()` (not yet
read), an IDF 4.4.7-specific bug, or a hardware interaction. This was not
investigated further because the outcome — WDT on both initialization paths
— was sufficient to confirm that IDF 4.4.7 cannot reliably support I2C slave
on this board, regardless of mechanism.

### Why the evidence is sufficient for the decision

The decision to switch platforms does not require explaining the
`i2c_driver_install()` WDT. The decision chain is:

1. MCU #3 requires I2C slave mode — confirmed requirement.
2. `Wire.begin()` in slave mode causes WDT — observed in testing.
3. Root cause for the Wire path — confirmed by source: internal task created
   at priority 20, starving IDLE1 on Core 1.
4. `i2c_new_slave_device()` is interrupt-driven and creates no internal task
   — confirmed by IDF 5.x design. This is the correct fix.
5. `i2c_new_slave_device()` requires IDF 5.2+ — confirmed fact.
6. IDF 4.4.7 does not provide it — confirmed fact.
7. The `i2c_driver_install()` WDT independently confirms that IDF 4.4.7's
   I2C slave support is unusable on this hardware, reinforcing point 6.
8. pioarduino provides arduino-esp32 3.x built against IDF 5.5.x, retaining
   Arduino APIs — confirmed fact.

The Wire path alone (points 1–6) is sufficient to justify the platform
switch. Point 7 adds weight but is not required.

### Why the current platform cannot fix this

The project currently uses `platform = espressif32@7.0.1` with
`framework = arduino, espidf` (combined mode). This platform bundles
arduino-esp32 core 2.x, which ships with IDF 4.4.7 compiled in. The
Arduino HAL is compiled against this specific IDF version — it cannot be
replaced independently.

`i2c_new_slave_device()`, the interrupt-driven slave API that creates no
internal task, was introduced in IDF 5.2. It is not available in IDF 4.4.7.

### Platform options evaluated

**Option A: Stay on espressif32@7.0.1, work around WDT**
Manually feed the watchdog (`esp_task_wdt_reset()`) or increase WDT timeout.
Rejected: masks the symptom without fixing the cause. Fragile in production
and misleading as a learning outcome.

**Option B: Switch to pure `framework = espidf`**
Drops all Arduino APIs. Requires rewriting Serial → `uart_driver_install`,
Wire/U8g2 → IDF I2C + custom OLED HAL, delay → `vTaskDelay`. Full control,
correct long-term solution. Rejected for now: significant rewrite scope
that delays the actual DB controller implementation. Remains viable for
Phase 5 cleanup.

**Option C: Switch to pioarduino platform (arduino-esp32 3.x, IDF 5.5.x)**
pioarduino is a community-maintained fork of the PlatformIO espressif32
platform that supports arduino-esp32 3.x. The latest stable release bundles
arduino-esp32 3.3.x built against IDF 5.5.x. This gives access to
`i2c_new_slave_device()` (IDF 5.2+) while retaining Arduino APIs (Serial,
Wire, U8g2). One `platformio.ini` line change for MCU #3 only.
Selected.

---

## Decision

Switch MCU #3 `platformio.ini` platform to pioarduino stable:

```ini
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
```

All other MCUs (#1, #2, #4, #5) remain on `espressif32@7.0.1`. The platform
difference is MCU #3 only — no shared library or shared config changes.

The shared bus library for MCU #3 (`shared_bus_wroom`) will use
`i2c_new_slave_device()` from `driver/i2c_slave.h` (IDF 5.2+ new driver).
This header must NOT be combined with `driver/i2c.h` (old driver) in the
same build — they conflict at link time.

### OLED under pioarduino

U8g2 `HW_I2C` compatibility with arduino-esp32 3.x under pioarduino is not
confirmed prior to implementation. There is a known pattern of U8g2 HW_I2C
breaking when the underlying Wire implementation changes between arduino-esp32
versions. A smoke test (Step 0.2) will determine whether HW_I2C works or
whether SW_I2C fallback is needed.

SW_I2C fallback path if needed:
- Switch constructor to `U8G2_SSD1306_128X64_NONAME_F_SW_I2C`
- Add `esp_log_level_set("gpio", ESP_LOG_WARN)` before `oled.begin()` to
  suppress GPIO driver INFO logs that previously caused UART TX FIFO
  spin-loops and WDT on WROOM

### shared_bus_wroom library interface

`shared_bus_wroom` exposes the same interface as `shared_bus` (used by
ESP32-C3 MCUs): `init()`, `send()`, `poll()`. Selected at compile time
via `#ifdef MCU_BOARD_WROOM`. Internally:

- `init()` — calls `i2c_new_slave_device()`, registers `on_receive` and
  `on_request` callbacks, creates FreeRTOS semaphore
- `poll()` — blocks task on semaphore; ISR callback gives semaphore on
  message receipt, copies bytes to internal buffer
- `send()` — calls `i2c_slave_write()` to pre-load TX buffer before master
  reads

---

## Impact on MCU #3

- `platformio.ini`: platform line changed to pioarduino zip URL
- `sdkconfig.defaults`: review required — some keys may differ between
  IDF 4.4.7 and IDF 5.5.x. `CONFIG_AUTOSTART_ARDUINO=y` remains required.
  `CONFIG_ARDUINO_RUNNING_CORE=0` was experimental — remove, revert to default (Core 1).
- `shared_bus_wroom.h/.cpp`: new files, using `driver/i2c_slave.h`
- `oled_display_wroom.cpp`: may need SW_I2C constructor swap pending smoke test
- `main.cpp`: no structural change — task architecture unchanged

## Impact on other MCUs

None. Platform change is isolated to MCU #3's `platformio.ini`.

---

## Consequences

- pioarduino is a community fork, not officially maintained by Espressif or
  PlatformIO. It is actively maintained (latest release 2026-01) but carries
  more risk than the official platform. Acceptable for a learning project.
- arduino-esp32 3.x includes breaking API changes from 2.x. These are
  unlikely to affect MCU #3 code (which uses only Wire, Serial, U8g2, and
  FreeRTOS primitives) but should be kept in mind if build errors appear.
- `driver/i2c.h` (old driver) must not be included anywhere in MCU #3 build
  once `driver/i2c_slave.h` is in use — conflict causes build failure.
- The `i2c_driver_install()` WDT root cause was not fully resolved. If the
  pure espidf path (Option B) is pursued in Phase 5, this should be
  investigated before assuming `i2c_driver_install()` is safe to use.
- Future reader: if pioarduino becomes unmaintained, Option B (pure espidf)
  remains the correct long-term path.

---

## Amendment note for ADR-009

ADR-009 pin table lists OLED GPIO16/17 as "U8g2 SW I2C". This was accurate
at the time of writing. During WDT investigation (prior to this ADR), OLED
was switched to hardware I2C (U8g2 HW_I2C, Wire bus 0 remapped to GPIO16/17)
to eliminate SW I2C bit-banging as a WDT source. ADR-009 pin table should
be read as: OLED GPIO16/17 = U8g2 HW_I2C (primary) / SW_I2C (fallback
pending pioarduino smoke test)
