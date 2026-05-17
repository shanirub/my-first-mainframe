# Glossary — Mainframe Simulation Project
*Accumulated from session: MCU #3 platform investigation and architecture design*

---

## Hardware & Chip Architecture

**ESP32-WROOM-32**
Dual-core ESP32 module (Xtensa LX6, Core 0 + Core 1). Has two hardware I2C
peripherals. GPIO8/9 available as general-purpose pins. Used for MCU #3.

**ESP32-C3 SuperMini**
Single-core RISC-V ESP32 variant. Has only one hardware I2C peripheral
(SOC_I2C_NUM=1). Used for MCUs #1, #2, #4, #5.

**Dual-core architecture**
CPU design with two independent processing cores. Each core runs its own
FreeRTOS scheduler instance and its own idle task. Relevant here because
a task monopolizing one core does not affect the other core — but does
starve that core's idle task independently.

**HAL (Hardware Abstraction Layer)**
Code layer sitting between a high-level API (e.g. Arduino's `Wire`) and
hardware-specific implementation (e.g. IDF's `i2c_driver_install()`).
Compiled against a specific IDF version — cannot be swapped independently
without recompiling the entire layer.

**GPIO matrix**
ESP32 internal routing layer that maps peripheral signals to physical GPIO
pins. Allows most peripherals to be assigned to most pins, unlike fixed-pin
designs.

**Open-drain**
Electrical configuration where a device can only pull a wire LOW or release
it. Wire goes HIGH passively via pull-up resistor. Fundamental to I2C —
multiple devices share SDA safely.

**Pull-up resistor**
Resistor connecting a signal line to VCC. Biases the line HIGH when nothing
actively drives it low. Required on I2C SDA and SCL lines.

**Pull-down resistor**
Resistor connecting a signal line to GND. Biases the line LOW when nothing
drives it.

---

## ESP-IDF & Framework

**ESP-IDF (Espressif IoT Development Framework)**
Native SDK for ESP32 chips. Provides drivers, FreeRTOS integration, and
hardware peripheral APIs. Version determines which APIs are available.

**IDF 4.4.7**
Bundled with arduino-esp32 core 2.x / PlatformIO espressif32@7.0.1.
`i2c_driver_install()` is the slave API. Does not have `i2c_new_slave_device()`.

**IDF 5.5.x**
Bundled with arduino-esp32 core 3.3.x / pioarduino stable.
Has `i2c_new_slave_device()` — interrupt-driven slave driver, no internal
task creation.

**Combined framework mode**
`framework = arduino, espidf` in platformio.ini. Keeps Arduino APIs while
exposing ESP-IDF internals. IDF version is locked to what the Arduino core
bundles — cannot be upgraded independently.

**`sdkconfig.defaults`**
Minimal ESP-IDF project config overrides file. Source of truth for build
configuration. Auto-generates full `sdkconfig` at build time. Only custom
lines go here — not the full generated config.

**`CONFIG_ARDUINO_RUNNING_CORE`**
sdkconfig key controlling which CPU core Arduino's `loopTask` is pinned to.
Default is Core 1. Was set to Core 0 experimentally during WDT investigation
— should be reverted to default.

**`CONFIG_AUTOSTART_ARDUINO`**
sdkconfig key required in combined framework mode. Tells IDF to start the
Arduino subsystem automatically on boot.

---

## I2C Driver

**`i2c_driver_install()`**
IDF 4.4.7 I2C driver init function. Used for both master and slave mode.
In slave mode, its internal behavior during initialization caused WDT on
WROOM — exact mechanism unconfirmed pending source code analysis.

**`i2c_new_slave_device()`**
IDF 5.2+ new I2C slave API. Interrupt-driven — registers an ISR that fires
only when the I2C peripheral receives data. Creates no internal FreeRTOS
task. Correct fix for the WDT problem. Available via `driver/i2c_slave.h`.

**`driver_ng`**
"Next generation" IDF I2C driver. The `ng` suffix indicates it is the
replacement for the legacy driver. `driver/i2c_slave.h` and `driver/i2c.h`
cannot coexist in the same build — including both causes a link error.

**`i2c_slave_task`**
Internal task name cited in community reports for the IDF 4.4 Arduino Wire
slave implementation. Claimed priority: 20. Unconfirmed from source code
— source grep found no `xTaskCreate` in IDF's `i2c.c`. May be in the
Arduino HAL layer instead. Requires further investigation next session.

**`i2cSlaveInit()`**
Internal Arduino Wire function that initializes I2C slave mode. Suspected
location of task creation causing WDT. Not yet confirmed from source code.

**`i2c_slave_read_buffer()`**
IDF 4.4.7 function to read received bytes from the I2C slave hardware FIFO.
Blocking with configurable timeout. Used in the test sketch's `loop()`.

**Hardware I2C (U8g2)**
U8g2 mode using the ESP32 hardware I2C peripheral instead of bit-banging.
Configured via `U8G2_SSD1306_128X64_NONAME_F_HW_I2C` constructor.

**Bit-banging**
Software simulation of a hardware protocol by manually toggling GPIO pins.
U8g2 uses this for software I2C. CPU-intensive — no hardware offloading.
On WROOM, caused WDT due to GPIO driver INFO log spam flooding UART TX FIFO.

**TwoWire**
Arduino ESP32 class implementing I2C. `Wire` = `TwoWire(0)`, `Wire1` =
`TwoWire(1)`. Pins assigned at `begin()` time.

---

## FreeRTOS Concepts

**FreeRTOS**
Real-Time Operating System running on ESP32. Provides tasks, queues,
semaphores, mutexes. Already running under Arduino — do not call
`vTaskStartScheduler()`.

**Task**
Independent unit of execution in FreeRTOS. Has its own stack, priority,
and state. Created with `xTaskCreate()` or `xTaskCreatePinnedToCore()`.

**Task states**
- **Running** — currently executing on a core
- **Ready** — eligible to run, waiting for CPU
- **Blocked** — waiting for an event (semaphore, queue, delay). Removed
  from ready list. CPU is freed for other tasks.
- **Suspended** — explicitly paused, not scheduled

**Blocking**
A task voluntarily removing itself from the ready list to wait for an event.
CPU is freed. Correct pattern for tasks waiting on I2C, UART, queues.
Opposite of busy-waiting.

**Busy-wait / spin-loop**
A task staying on the ready list while waiting, repeatedly checking a
condition without yielding. CPU is consumed even though no useful work is
done. Starves lower-priority tasks on the same core.

**Yielding**
A task voluntarily returning control to the scheduler. `vTaskYield()` moves
the task to the back of the ready list at its own priority — scheduler picks
highest priority ready task next. Does NOT guarantee lower-priority tasks run.
Distinct from blocking — the task stays in the ready list.

**`vTaskDelay()`**
FreeRTOS call that blocks a task for N ticks, removing it from the ready
list. Allows lower-priority tasks and idle task to run. Not the same as
`vTaskYield()`.

**`vTaskYield()`**
FreeRTOS call that passes execution to the next ready task at same or higher
priority. Does not guarantee idle task runs. Does not block the calling task.

**Preemption**
Scheduler forcibly interrupting a running task to switch to a higher-priority
ready task. Triggered by the FreeRTOS tick interrupt. Only switches to equal
or higher priority — cannot preempt to run a lower-priority task.

**Priority**
Integer value (0 = lowest) assigned to a task at creation. FreeRTOS always
runs the highest-priority ready task on each core. IDLE tasks are priority 0.

**Priority inheritance**
FreeRTOS mechanism for mutexes only: if a high-priority task waits for a
mutex held by a low-priority task, the low-priority task temporarily inherits
the higher priority to finish faster. Does NOT apply generally — FreeRTOS
has no general priority aging.

**Priority aging**
Algorithm where a repeatedly-skipped task's priority increases over time to
prevent permanent starvation. FreeRTOS does NOT implement this (outside of
mutex priority inheritance). Developer is responsible for not starving tasks.

**Core affinity**
FreeRTOS concept of pinning a task to a specific CPU core via
`xTaskCreatePinnedToCore()`. `tskNO_AFFINITY` lets the scheduler choose.

**`loopTask`**
Arduino's main task wrapping `setup()` and `loop()`. Pinned to Core 1 by
default on ESP32-WROOM-32. Relevant: if `setup()` blocks without yielding,
IDLE1 on Core 1 is starved.

**IDLE task (IDLE0 / IDLE1)**
Lowest-priority FreeRTOS task (priority 0), one per core. Runs only when no
other task on that core is ready. Performs FreeRTOS housekeeping: stack
overflow checking, memory cleanup after `vTaskDelete()`, power management
(`esp_pm_impl_waiti`). TWDT monitors it as a scheduler health signal.

**`uxTaskGetStackHighWaterMark()`**
FreeRTOS function returning the minimum remaining stack space a task has had
since creation. Used to detect stack overflow risk. Returns 0 = overflow
imminent.

**`xTaskCreate()` / `xTaskCreatePinnedToCore()`**
FreeRTOS functions to create tasks. `PinnedToCore` variant locks the task
to a specific core. Unpinned tasks use `tskNO_AFFINITY`.

---

## Watchdog Timers

**TWDT (Task Watchdog Timer)**
Built on Timer Group 0. Monitors subscribed tasks — by default both idle
tasks (IDLE0, IDLE1). Fires if any subscribed task does not get CPU time
within the timeout period (default ~5 seconds). Used to detect tasks running
without yielding. What fired in our case.

**IWDT (Interrupt Watchdog Timer)**
Built on Timer Group 1. Monitors the FreeRTOS tick interrupt on each CPU.
Fires if tick interrupt is not serviced within timeout — indicates ISRs are
blocked. Different from TWDT. Panic message: "Interrupt wdt timeout on CPU0/1".

**TG1WDT**
Timer Group 1 Watchdog — the hardware timer underlying the IWDT. Sometimes
used loosely in community reports to refer to WDT resets generally.

**`_xt_context_save`**
Xtensa CPU routine saving registers during FreeRTOS task switch. Appearing
in WDT backtrace indicates WDT fired during a context switch — suggests
a new task was being created or switched to. Observed in our backtrace when
`i2c_driver_install()` caused WDT.

**`esp_task_wdt_reset()`**
Function to manually feed (reset) the TWDT from within a subscribed task.
Using this to work around starvation masks the symptom without fixing the
cause — not a recommended solution.

---

## Watchdog Root Cause Investigation (Ongoing)

**Observed behavior (confirmed):**
- `Wire.begin()` in slave mode → WDT fires on WROOM, not on ESP32-C3
- `i2c_driver_install()` in slave mode → WDT fires during the call itself
  (never returned ESP_OK, "slave active" print never appeared)
- WDT fires with only `loopTask` running — no user tasks created

**Unconfirmed claims (require source code verification next session):**
- `i2c_slave_task` created at priority 20 — not found in IDF `i2c.c` grep
- Whether task creation is in Arduino HAL (`esp32-hal-i2c-slave.c`) instead
- Whether `i2c_driver_install()` busy-waits internally during slave init

**Next session commands:**
```bash
find ~/.platformio -name "*i2c*slave*" 2>/dev/null

grep -rni "slave_task\|xTaskCreate\|i2cSlaveInit" \
  ~/.platformio/packages/framework-arduino-esp32/cores/esp32/esp32-hal-i2c-slave.c

grep -n "while\|vTaskDelay\|xSemaphoreTake\|xTaskCreate\|spin\|wait" \
  ~/.platformio/packages/framework-espidf/components/driver/i2c.c | head -80
```

---

## Software Architecture & Patterns

**Task-per-peripheral ownership**
Design pattern where each hardware peripheral is owned exclusively by one
task. No shared access, no mutex needed on the peripheral itself. Used in
MCU #3: Receiver owns I2C slave poll, UART task owns Serial2, OLED task
owns display.

**Mediator pattern**
Design pattern where one component (Logic task) coordinates between others
without them knowing about each other. UART task doesn't know about the bus.
Receiver task doesn't know about UART.

**Bridge pattern**
Software design pattern separating abstraction from implementation so both
can vary independently. Used here: same SharedBus interface (`init()`,
`send()`, `poll()`), different WROOM/C3 implementations selected via
`#ifdef MCU_BOARD_WROOM`.

**Conditional compilation**
Using `#ifdef`/`#ifndef` preprocessor directives to include different code
paths at compile time based on build flags (e.g. `MCU_BOARD_WROOM`).

**ISR (Interrupt Service Routine)**
Function called directly by hardware when an event occurs (e.g. I2C data
received). Must be short and must not block. Can signal a FreeRTOS task via
`xSemaphoreGiveFromISR()` or `xQueueSendFromISR()`.

**ISR-to-task signaling**
Pattern where an ISR gives a semaphore, and a task blocks taking that
semaphore. Task wakes only when the hardware event occurs. Correct
alternative to busy-waiting for hardware events.

**Semaphore**
FreeRTOS synchronization primitive. Binary semaphore: given by ISR when
event occurs, taken by task to wake up. Task blocks (CPU freed) while waiting.

**Mutex (Mutual Exclusion Lock)**
FreeRTOS primitive protecting a shared resource from concurrent access.
`displayMutex` protects `SharedState` between Logic task (writer) and
OLED task (reader). Unlike semaphores, mutexes have ownership and support
priority inheritance.

**Queue (FreeRTOS)**
Thread-safe FIFO pipe between tasks. Producer puts items in, consumer takes
items out. Consumer blocks if queue is empty — CPU freed. Used: `inboundQueue`,
`uartQueue`, `uartResultQueue`.

**`inboundQueue`**
Queue carrying decoded bus messages from Receiver task to Logic task.
Decouples ISR-side reception from application-side decision making.

**`uartQueue` / `uartResultQueue`**
Paired queues forming a request/response channel between Logic task and
UART task. Logic submits a request and blocks waiting for result. UART task
processes and replies.

**`displayMutex`**
Mutex protecting `SharedState` struct from concurrent access between Logic
task (writer) and OLED task (reader). Prevents display corruption from
mid-update reads.

**SharedState**
Struct shared between Logic task and OLED task under `displayMutex`.
Contains `readCount`, `writeCount`, `lastAccount`, `lastError`.

**Write-ahead log (WAL)**
Pattern where a transaction is written to a log as PENDING before updating
the actual data, then marked COMMITTED after success. On RPi: enables
crash recovery — PENDING entries on boot indicate interrupted transactions.

**Stack overflow**
Condition where a task uses more stack memory than allocated. Can cause
silent corruption or crash. Detected by `uxTaskGetStackHighWaterMark()` or
FreeRTOS stack canary checking (performed by idle task).

---

## Platform & Tooling

**PlatformIO**
Build system and IDE extension for embedded development. Manages platforms,
frameworks, libraries, upload/monitor ports.

**pioarduino**
Community fork of PlatformIO's espressif32 platform supporting
arduino-esp32 3.x (IDF 5.5.x). Not officially maintained by Espressif or
PlatformIO. Actively maintained as of 2026. Used for MCU #3 only.

**espressif32@7.0.1**
Official PlatformIO platform. Bundles arduino-esp32 core 2.x / IDF 4.4.7.
Used for MCUs #1, #2, #4, #5.

**Smoke test**
Minimal first build to verify a platform or component compiles and boots
correctly before adding application logic.

**ADR (Architecture Decision Record)**
Document recording a significant technical decision: context, options
considered, decision made, consequences. Numbered sequentially. Immutable
once accepted — superseded by a new ADR rather than edited.

**FMEA (Failure Mode and Effects Analysis)**
Structured method for identifying what can go wrong in a system, how likely
it is, and what the impact and mitigation are. ADR-011 skeleton created as
placeholder — to be completed after Phase 3 is running end-to-end.

**`esp_log_level_set()`**
ESP-IDF runtime function to set log verbosity per tag.
`esp_log_level_set("gpio", ESP_LOG_WARN)` suppresses INFO messages from the
GPIO driver — required when using U8g2 SW_I2C on WROOM to prevent GPIO log
spam flooding UART TX FIFO and causing spin-loops.

**UART TX FIFO**
Hardware queue for outgoing serial bytes. Limited size. If full,
`uart_write()` spin-loops waiting for space — blocks the calling task
entirely without yielding.

**Spin-loop (busy-wait)**
Code that repeatedly checks a condition without yielding. Example: waiting
for UART TX FIFO space. Blocks the CPU entirely. Starves lower-priority
tasks on same core.
