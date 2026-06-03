# ADR-011: Raspberry Pi 3B+ replaces MCU #3 on shared bus

## Status
Accepted — 2026-06-03
Supersedes ADR-009 (RPi UART backend) and ADR-010 (pure ESP-IDF framework)

---

## Context

MCU #3 (ESP32-WROOM-32 DevKit) was designed to be the Database Controller
subsystem on the shared inter-MCU I2C bus at address 0x0A. Its role was to
receive DB_READ and DB_WRITE requests from other MCUs, proxy them to a
Raspberry Pi 3B+ over UART, and return results on the shared bus.

The blocking issue was Phase 3.1: initializing an I2C slave on GPIO8/9 via
`i2c_new_slave_device()` (IDF 5.4.0, slave driver v2). Every attempt caused
a `TG1WDT_SYS_RESET` before the function returned.

### Investigation history — what was tried across all sessions

**Arduino Wire.begin() in slave mode (ADR-010)**
Caused TG1WDT on both Wire (bus 0) and Wire1 (bus 1). Root cause confirmed
from source code: `i2cSlaveInit()` creates an internal FreeRTOS task at
priority 20, which starves IDLE1 on the dual-core WROOM and triggers the
Task Watchdog. Not fixable without modifying the framework.

**IDF 4.4.7 i2c_driver_install() in slave mode (ADR-010)**
Same TG1WDT result. Root cause not fully traced. Disqualified.

**pioarduino (arduino-esp32 3.x, IDF 5.5.x) with i2c_new_slave_device() (ADR-010)**
arduino-esp32 auto-initializes Wire on GPIO8/9 before setup() runs, claiming
those pins in the IDF GPIO matrix before user code can execute. Three
workarounds attempted; all failed. The Wire auto-init is a framework-level
constraint that cannot be suppressed from user code.

**Pure ESP-IDF (framework = espidf, IDF 5.4.0) with i2c_new_slave_device() (ADR-010, Phase 3.1)**
Eliminated the Arduino GPIO conflict at root. i2c_new_slave_device() still
caused TG1WDT_SYS_RESET on every boot before returning. Extensive isolation
testing ruled out: bus activity from other MCUs, concurrent OLED task, callback
registration, mutex contention, and GPIO idle state.

Investigation into the hang location revealed:
- Both CPUs were inside panicHandler() at WDT reset time (addr2line confirmed)
- A panic fires inside i2c_new_slave_device() before it returns
- The panic handler deadlocks on the dual-core inter-core stall mechanism
  before printing anything, then the WDT fires again and resets the chip
- CONFIG_ESP_SYSTEM_PANIC_PRINT_HALT was correctly set but ineffective
  because the WDT fires before the panic handler completes output
- Disabling CONFIG_ESP_TASK_WDT_INIT produced complete silence — no output
  at all, indicating the panic handler itself halts before reaching UART
- Single-core mode (CONFIG_FREERTOS_UNICORE=y) also produced no output

Source code investigation established:
- SOC_I2C_SUPPORT_SLAVE=1 on plain ESP32 — slave is supported at SOC level
- SOC_I2C_SLAVE_CAN_GET_STRETCH_CAUSE is absent on plain ESP32
- The entire v2 slave test suite (test_i2c_slave_v2.c) is guarded by
  SOC_I2C_SLAVE_CAN_GET_STRETCH_CAUSE — Espressif never runs v2 slave
  tests on plain ESP32
- The official i2c_slave_network_sensor example explicitly excludes plain
  ESP32 from its supported targets
- Stretch LL functions are empty stubs on plain ESP32 — not the hang cause
- The exact line inside i2c_new_slave_device() where the panic originates
  remains unidentified due to the panic handler output failure

The next diagnostic step would be adding ESP_LOGI prints directly into
i2c_slave_v2.c to narrow the hang to a single statement. This is technically
feasible but represents continued investment in a path with no confirmed
end — each finding has revealed a new unknown rather than a fix.

### Decision to stop

MCU #3 I2C slave initialization has consumed weeks of debugging time across
multiple boards (ESP32-C3 SuperMini → WeMos LOLIN32 Lite → ESP32-WROOM-32
DevKit), multiple frameworks (Arduino → pioarduino → pure ESP-IDF), and
multiple driver versions (Wire → i2c_driver_install → i2c_new_slave_device).
Each pivot was justified and well-documented. The current blocker is deep
inside an untested IDF driver path on a chip variant Espressif does not
officially validate for this use case.

The cost of continuing to chase this is no longer justified for a learning
project. The pivot to Raspberry Pi is a deliberate architectural decision,
not a workaround.

### Why RPi as I2C slave was previously rejected (ADR-009)

ADR-009 rejected the RPi-as-I2C-slave option with the reasoning:
"Linux I2C slave driver support on RPi is notoriously unreliable and would
likely reproduce the same debugging spiral in a different form."

That concern remains valid. However, the comparison has changed. The ESP32
debugging environment — no working panic output, WDT masking all diagnostics,
untested driver paths — is maximally opaque. Linux, by contrast, provides
dmesg, strace, readable error messages, and mature Python I2C libraries.
If Linux I2C slave proves difficult, the debugging surface is far more
transparent. Additionally, software I2C (bit-banging via pigpio or a similar
library) is a viable fallback on Linux that has no equivalent on the ESP32
path we were on.

The risk of a new debugging spiral exists. It is accepted as preferable to
continuing the current one.

---

## Decision

The Raspberry Pi 3B+ replaces MCU #3 (ESP32-WROOM-32) entirely on the
shared inter-MCU I2C bus. The RPi joins the bus at address 0x0A and takes
over the Database Controller role directly.

The ESP32-WROOM-32 DevKit is retired from this project and returned to stock.
It may be reused in a future project.

The UART link between MCU #3 and the RPi (ADR-009) is no longer needed —
the RPi owns both the I2C slave role and the SQLite storage directly.

All other MCUs (#1, #2, #4, #5) are unaffected. They continue to send
DB_READ and DB_WRITE to address 0x0A and receive DB_READ_RESULT and
DB_WRITE_ACK in return. The change is invisible to the rest of the bus.

---

## What is decided

- RPi 3B+ is the Database Controller at I2C address 0x0A
- RPi owns SQLite storage and Flask web interface directly (no UART proxy)
- ESP32-WROOM-32 DevKit is retired from the project
- The shared bus address 0x0A is preserved — no changes to other MCUs

## What is deferred to the next session

The following require research and a dedicated design session before any
hardware changes or code is written:

- Which RPi GPIO pins connect to the shared bus SDA/SCL
- Whether to use kernel I2C slave driver or software I2C (pigpio or similar)
- Whether the RPi OLED (if retained) conflicts with the shared bus I2C pins
- Python library selection and I2C slave implementation approach
- Revised roadmap entries for MCU #3 phases 3–6

No hardware should be rewired and no code should be written until the
design session produces a verified plan.

---

## Impact on existing documents

- ADR-009: superseded by this ADR. The UART protocol design it contains
  is void — the UART link no longer exists.
- ADR-010: superseded by this ADR. The pure ESP-IDF framework decision
  and shared_bus_wroom implementation are void.
- roadmap.md: MCU #3 phases 3.1–6 are on hold pending the design session.
- requirements.md: no functional requirements change — DB_READ, DB_WRITE,
  and web interface are still required, now delivered by RPi directly.
- shared_config.h: no changes — bus address 0x0A and GPIO8/9 pin constants
  are unchanged.

---

## Consequences

- MCU #3 firmware (shared_bus_wroom, oled_display_wroom, main.cpp) is
  preserved in the repo but inactive. Do not delete — it documents the
  investigation and may be referenced in future sessions.
- The UART link hardware (GPIO18/19 on MCU #3, GPIO14/15 on RPi) is
  no longer needed and can be unwired after the new RPi design is confirmed.
- RPi now has two hardware responsibilities: I2C slave on shared bus, and
  SQLite + Flask. Both run on the same device — this is simpler than the
  MCU #3 + RPi split, not more complex.
- Phase 5 UART task items in the roadmap (5.1–5.5) are obsolete and should
  be removed or replaced in the next roadmap update.