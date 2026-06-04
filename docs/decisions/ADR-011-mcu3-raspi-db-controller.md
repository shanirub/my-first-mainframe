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
dmesg, strace, readable error messages, and mature Python libraries.
If Linux I2C slave proves difficult, the debugging surface is far more
transparent.

Additionally, the BCM2837 SoC on the RPi 3B+ contains a dedicated BSC
(Broadcom Serial Controller) slave peripheral, accessible via the pigpio
library's `bsc_i2c()` function. This is hardware-backed slave support —
not bit-banging — and has confirmed working reports on RPi 3 hardware
communicating with Arduino masters. The risk of a new debugging spiral
exists and is accepted as preferable to continuing the current one.

---

## Decision

The Raspberry Pi 3B+ replaces MCU #3 (ESP32-WROOM-32) entirely on the
shared inter-MCU I2C bus. The RPi joins the bus at address 0x0A and takes
over the Database Controller role directly.

The ESP32-WROOM-32 DevKit is retired from this project. See ADR-008 for
its hardware history.

The UART link between MCU #3 and the RPi (ADR-009) is no longer needed.
Existing UART wiring (RPi GPIO14/15) should be removed after the new
I2C wiring is confirmed working.

All other MCUs (#1, #2, #4, #5) are unaffected. They continue to send
DB_READ and DB_WRITE to address 0x0A and receive results in return.
The change is invisible to the rest of the bus.

---

## Hardware

### RPi I2C slave (shared bus)

The BCM2837 BSC slave peripheral is hardwired to GPIO18 (SDA) and GPIO19
(SCL) — these pin assignments are fixed in silicon and cannot be changed
in software. The RPi connects to the shared bus hub via these two pins.

```
Shared bus hub (SDA/SCL rails)
    │
    ├── MCU #1  GPIO8/9   (existing)
    ├── MCU #2  GPIO8/9   (existing)
    ├── MCU #4  GPIO8/9   (existing)
    ├── MCU #5  GPIO8/9   (existing)
    └── RPi     GPIO18/19 (BSC slave — new)

Pull-ups: existing 5kΩ on hub SDA and SCL — unchanged.
Voltage: RPi GPIO18/19 are 3.3V — matches ESP32-C3. No level shifting needed.
```

### RPi OLED (private display)

A plain SSD1306 128×64 OLED connects to the RPi via the BSC1 master
peripheral on GPIO2 (SDA) and GPIO3 (SCL). This is a separate wire pair
from the shared bus and does not connect to the hub.

```
RPi GPIO2/3 → SSD1306 OLED (private, not on shared bus hub)
```

GPIO18/19 (BSC slave) and GPIO2/3 (BSC1 master) are two separate hardware
peripherals on the BCM2837. They do not interfere with each other.

---

## Protocol

### Why the RPi never needs to initiate

In the existing SharedBus design, MCUs switch between slave (listening) and
master (sending) roles at runtime. When a slave MCU responds to a message,
it temporarily becomes the bus master, addresses the target, and pushes
the response.

The RPi does not need this pattern. Every message the RPi sends is a
response to a prior request from an MCU. The I2C protocol supports this
without the RPi ever generating a START condition: the requesting MCU
remains master for the entire exchange, using a master-read transaction
to clock out the RPi's response from the BSC slave TX FIFO.

This means the RPi operates as a pure I2C slave throughout. The BSC slave
peripheral is sufficient — no master role is needed on the shared bus.

### Request-response flow

Every RPi interaction follows this two-transaction pattern:

```
MCU (master write):   START → addr 0x0A + W → payload bytes → STOP
                      [RPi processes request, loads response into TX FIFO]
MCU (master read):    START → addr 0x0A + R → clocks out 256 bytes → STOP
```

The MCU drives SCL for both transactions. The RPi drives SDA only during
the read transaction, from its TX FIFO.

### Readiness signalling

The RPi cannot pause the master's read clock. If the TX FIFO is not yet
loaded when the MCU issues the read, the BSC slave returns 0xFF bytes.
To avoid this, a 1-byte ready flag precedes all responses:

- `0x00` — not ready, try again
- `0x01` — response follows in bytes 1–255

The MCU polls `requestFrom(0x0A, 256)` in a loop with short delays until
byte[0] == 0x01 or the 500ms timeout elapses. On timeout, the MCU returns
`Status::TIMEOUT` to the caller, consistent with existing error handling.

The MCU always reads exactly 256 bytes — the maximum I2C buffer size
already used throughout the system. Trailing bytes beyond the JSON payload
are ignored by the parser.

### DB_READ flow

```
MCU #2                              RPi (pigpio BSC slave, GPIO18/19)
  │                                         │
  │── send(0x0A, DB_READ) ─────────────────►│  write transaction
  │                                         │  pigpio EVENT_BSC fires
  │                                         │  parse request
  │                                         │  query SQLite (Phase 4+)
  │                                         │  load DB_READ_RESULT into TX FIFO
  │                                         │  set ready_byte = 0x01
  │                                         │
  │  ┌── poll loop (500ms timeout) ─────────┤
  │  │  requestFrom(0x0A, 256) ────────────►│  read transaction
  │  │  check byte[0]                       │  RPi drives SDA from TX FIFO
  │  │  0x00 → vTaskDelay(10ms), retry      │
  │  │  0x01 → break                        │
  │  └──────────────────────────────────────┘
  │                                         │
  │  parse bytes[1..255] → DB_READ_RESULT   │
  │  [timeout → Status::TIMEOUT]            │
```

### DB_WRITE flow

```
MCU #2                              RPi
  │                                         │
  │── send(0x0A, DB_WRITE) ────────────────►│  write transaction
  │                                         │  write WAL entry (PENDING)
  │                                         │  update balance
  │                                         │  mark COMMITTED
  │                                         │  load DB_WRITE_ACK into TX FIFO
  │                                         │  set ready_byte = 0x01
  │                                         │
  │  ┌── poll loop (500ms timeout) ─────────┤
  │  │  requestFrom(0x0A, 256) ────────────►│
  │  │  check byte[0] → break on 0x01       │
  │  └──────────────────────────────────────┘
  │                                         │
  │  parse bytes[1..255] → DB_WRITE_ACK     │
```

### HEARTBEAT flow

```
MCU #1                              RPi
  │                                         │
  │── send(0x0A, HEARTBEAT) ───────────────►│  write transaction
  │                                         │  load HEARTBEAT_ACK into TX FIFO
  │                                         │  set ready_byte = 0x01
  │                                         │  (no SQLite — fixed response)
  │                                         │
  │  vTaskDelay(20ms)                       │  [fixed delay: no SQLite latency]
  │  requestFrom(0x0A, 256) ───────────────►│
  │  parse bytes[1..255] → HEARTBEAT_ACK    │
```

HEARTBEAT_ACK requires no database query. A fixed 20ms delay before the
single read attempt is sufficient to cover pigpio callback latency.
No polling loop is needed.

---

## MCU firmware changes required

### New SharedBus method: sendAndReceive()

```cpp
// Sends a message to target, then polls requestFrom() until the ready
// byte (byte[0]) == 0x01 or timeoutMs elapses. On success, reads exactly
// 256 bytes into rxBuf. Returns Status::OK or Status::TIMEOUT.
//
// busMutex is held across the full send + poll sequence — no other task
// can interleave a bus operation between the write and read transactions.
uint8_t sendAndReceive(uint8_t target,
                       const char* txMsg,
                       char* rxBuf,
                       uint32_t timeoutMs = 500);
```

Required on: MCU #2 (DB_READ, DB_WRITE), MCU #1 (HEARTBEAT).

MCU #2 logic task replaces the pattern:
```
send(DB_READ) → block on inboundQueue waiting for DB_READ_RESULT
```
with:
```
sendAndReceive(DB_READ, rxBuf) → parse rxBuf directly
```

The inboundQueue path (poll → receive pushed result) is not used for
RPi interactions. The RPi never pushes — it only responds to reads.

---

## RPi software architecture

See `docs/design/raspi_architecture.md` for the full thread design.

Summary: three Python threads (bus_worker, oled_worker, flask — Phase 4+)
communicating via `queue.Queue` and `threading.Lock`, mirroring the
FreeRTOS task + queue pattern used on the MCUs. The pigpio EVENT_BSC
callback is a producer that puts incoming requests onto the bus queue;
the bus_worker thread owns all SQLite access and TX FIFO loading.

---

## Repo structure

```
code/
  raspi-db-server/
    CLAUDE.md           — RPi setup steps, design notes, DoD per phase
    src/
      bus_slave.py      — pigpio bsc_i2c setup, EVENT_BSC callback, TX FIFO protocol
      db.py             — SQLite read/write, WAL mode (Phase 4+)
      web_server.py     — Flask transaction history at :5000 (Phase 4+)
      oled.py           — luma.oled SSD1306 driver on GPIO2/3
    schema/
      schema.sql        — accounts + transactions tables (Phase 4+)
```

---

## What is decided

- RPi 3B+ is the Database Controller at I2C address 0x0A
- RPi connects to shared bus via GPIO18/19 (BSC slave, pigpio bsc_i2c)
- RPi OLED connects via GPIO2/3 (BSC1 master, separate from shared bus)
- RPi owns SQLite storage and Flask web interface directly (Phase 4+)
- ESP32-WROOM-32 DevKit is retired — see ADR-008 for hardware history
- Shared bus address 0x0A is preserved — no changes to other MCUs
- sendAndReceive() added to SharedBus for MCU #1 and MCU #2

## What is deferred

- SQLite implementation (db.py) — Phase 4
- Flask web interface (web_server.py) — Phase 4
- Crash recovery (PENDING transaction replay) — Phase 5
- rsync backup to dev PC — Phase 4

---

## Impact on existing documents

- ADR-009: superseded. UART protocol and wiring are void.
- ADR-010: superseded. ESP-IDF framework decision and shared_bus_wroom
  implementation are inactive. Code preserved in repo for reference.
- roadmap.md: MCU #3 phases replaced per this ADR.
- requirements.md: no functional requirement changes.
- shared_config.h: no changes — bus address 0x0A unchanged.
- CLAUDE.md (MCU #3): retired entry. RPi has its own CLAUDE.md.

---

## Consequences

- MCU #3 firmware (shared_bus_wroom, oled_display_wroom, main.cpp) is
  preserved in the repo but inactive. Do not delete — documents the
  investigation and may be referenced in future sessions.
- UART wiring (RPi GPIO14/15 ↔ former MCU #3 GPIO18/19) should be
  removed after new I2C wiring is confirmed working.
- RPi now has two hardware responsibilities: I2C slave on shared bus
  (GPIO18/19), and SSD1306 OLED (GPIO2/3). Both on the same device —
  simpler than the MCU #3 + RPi split, not more complex.
- MCU #2 and MCU #1 logic tasks require minor changes to use
  sendAndReceive() instead of send() + poll() for RPi interactions.
- RPi must be booted before MCU #1 and MCU #2 start processing —
  both handle RPi-down gracefully via the 500ms timeout.