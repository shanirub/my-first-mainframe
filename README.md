# 🖥️ Low-Level Mainframe Simulation Using MCUs

> Simulating a banking mainframe architecture using four ESP32-C3 microcontrollers and a
> Raspberry Pi 3B+ — because understanding distributed systems is way more fun when you
> can hold the hardware in your hands.

---

## 🪦 Project status: RETIRED — frozen at ADR-012 (2026-06-18)

**This repository is closed.** The shared-I2C-bus architecture it was built around
(ADR-008 → ADR-011) has been retired *in full* by the terminal decision record
[ADR-012](docs/decisions/ADR-012-retire-shared-bus-begin-mainframe-core-redesign.md).
Work continues in a **successor project** with a different inter-device transport and a
re-centred architecture (see [Why it was retired](#-why-it-was-retired) and
[The successor](#-the-successor) below).

Nothing here is deleted. The firmware, the libraries, and the BSC slave code are preserved
as the **investigation record** — the reasoning that led to the redesign is the point of
keeping them.

| | |
|---|---|
| **Terminal ADR** | ADR-012 — *Retire the shared-bus subsystem architecture; begin mainframe-core redesign* |
| **Retired** | The shared I2C inter-device bus, the "co-equal subsystems on one bus" premise, the RPi BSC slave |
| **Still in use** | I2C for the *private* per-device OLED displays only |
| **Carried forward** | Subsystem decomposition, message semantics, FreeRTOS task pattern, SQLite/WAL durability plan, all physical hardware |

---

## 📖 What this project was

A low-level simulation of a banking mainframe built from four ESP32-C3 microcontrollers and
a Raspberry Pi 3B+, each device mapped to a distinct mainframe subsystem. The devices
communicated over a **shared I2C bus** — a single two-wire bus (SDA/SCL) shared by all
devices — chosen to mirror the channel-subsystem architecture of IBM z/OS mainframes.

Each subsystem ran autonomously and coordinated with its peers to process banking
transactions (deposits, withdrawals, transfers, balance queries). The build was
deliberately low-level: no frameworks or abstractions beyond what the project built itself,
with every design decision documented in the devlog and the ADRs (Architecture Decision
Records — short documents that capture *what* was decided and *why*).

The project was educational in intent but rigorous in execution. Real hardware constraints
were discovered through testing and resolved through principled architectural decisions —
and ultimately one of those decisions was to **stop**, which is what ADR-012 records.

---

## 🧭 Why it was retired

The retirement was an *architectural* decision, not a workaround. The recurring failures
were diagnosed as a **wrong-bus-for-the-role mismatch**, and the correct response was judged
to be changing the architecture that forced those roles — not patching the wire beneath it
again.

### The structural mismatch

I2C is a primary/secondary (historically "master/slave") bus: well suited to one controller
talking to simple peripherals. This project pressed it into **two roles it suits poorly**:

1. **Multi-master peer messaging with runtime mode-switching** — devices flipping between
   controller and target roles at runtime to form a heartbeat/ACK mesh. Documented as very
   difficult to stabilise.
2. **Target ("slave") mode on devices with weak target support** — the ESP32 I2C-slave
   initialisation path (watchdog resets, `TG1WDT_SYS_RESET`, inside untested IDF driver
   paths) and the Linux/BSC slave on the Raspberry Pi.

This was the **fourth manifestation of one underlying mismatch**. ADR-008 (SD card → three
board swaps), ADR-009 (UART backend), ADR-010 (switch to pure ESP-IDF), and ADR-011 (RPi as
BSC slave) were each well-reasoned escapes from the *same* trap — and each **moved** the
failure rather than removing it. Continuing to patch the transport was continuing to fight
the bus.

### The immediate trigger (Phase 3.3) — stated with calibrated confidence

The proximate cause surfaced at Phase 3.3: the **BCM2837 BSC** (Broadcom Serial Controller)
slave peripheral that `pigpio bsc_i2c()` drives has a **16-byte hardware FIFO** (a small
first-in-first-out buffer in silicon), which conflicts with the 256-byte fixed-buffer TX
protocol the design assumed.

Two distinct questions came out of investigating the `pigpio` source (`bscXfer()` in
`pigpio.c`), and they resolved differently:

- **Message size — closed.** The 16-byte FIFO is *not* a hard message-size limit. `pigpio`
  drains it in a software busy-loop into a 512-byte buffer while the transaction is active,
  so a chunking protocol is not *strictly* forced.
- **Reliability — not closed.** The servicing is **PIO** (programmed I/O — the CPU
  hand-copies each byte) with **no DMA** (no direct-memory-access engine to move bytes
  without the CPU). A message larger than 16 bytes at 400 kHz arrives intact only if the
  daemon is already servicing the FIFO *during* the transaction; the 16-byte slack overruns
  in roughly **360 µs**. Reliable reception is therefore a **timing gamble on a loaded Linux
  host**, not a guarantee.

The reliability question was judged **not worth resolving empirically**, because the
structural mismatch above made the whole transport the wrong foundation regardless of the
outcome of one timing test.

> *Note on a cross-reference:* the previous README attributed the MCU #3 / ESP32 I2C-slave
> retirement to ADR-011. ADR-012's own account maps the ESP32 slave-init watchdog issue to
> **ADR-010** and ADR-011 to the **RPi-as-BSC-slave** decision. This README follows ADR-012
> as the authoritative consolidating record; the exact inner ADR numbering can be reconciled
> against ADR-008–011 directly.

---

## 🚀 The successor

A new project replaces this one. ADR-012 deliberately does **not** elaborate the successor's
design — *it will have its own ADR-001 and design documents* — so only the committed
direction is recorded here (anything beyond this would be speculation):

- The **Raspberry Pi becomes the mainframe central complex**: the CICS-analogue
  (transaction monitor), the transaction processor, and the job scheduler run on the Pi as
  **software subsystems**, and durable storage (SQLite) is **local to the Pi**.
- The four **ESP32-C3 MCUs become channel-attached peripherals** — operator console,
  terminals, I/O channels — rather than co-equal bus subsystems.
- The **inter-device transport is reselected** from a bus suited to the role (the specific
  choice belongs to the successor's docs, not here).

---

## 📦 What carries forward

The transport was the failure; the system above it was not. These are sound and migrate to
the successor as concepts:

- The **subsystem decomposition** and the **CICS-minimal transaction flow**
  (deposit / withdraw / balance → durable storage → acknowledge).
- The **message semantics**: `HEARTBEAT`/`ACK`, `DB_READ`/`DB_WRITE`, job
  submit/dispatch/complete.
- The **FreeRTOS task pattern** on the MCUs and the **SQLite/WAL** (write-ahead logging — a
  journaling mode that lets storage survive a mid-write crash) durability plan.
- **All physical hardware**: the 4 ESP32-C3 MCUs, the RPi 3B+, the 5 OLEDs, the power
  arrangement, the breadboards, and the logic analyzer.

The functional goal is unchanged: a minimal CICS analogue with durable, reboot-surviving
storage. Only the architecture that delivers it changes.

---

## 🏛️ Architecture as built (historical record)

> Everything below documents the **retired** shared-bus design as it stood when the project
> was frozen. It is kept for reference, not as a description of current or future work.

Four ESP32-C3 microcontrollers and one Raspberry Pi 3B+, each mapped to a real mainframe
subsystem:

| MCU | Role | Mainframe Equivalent | I2C Address |
|-----|------|---------------------|-------------|
| #1 | Master Console | System Console / Operator Interface | `0x08` |
| #2 | Transaction Processor | Central Processor (CP) | `0x09` |
| RPi 3B+ | Database Controller | DASD Controller | `0x0A` |
| #4 | Job Scheduler | JES (Job Entry Subsystem) | `0x0B` |
| #5 | I/O Controller | Channel Subsystem | `0x0C` |

> MCU #3 (ESP32-WROOM-32 DevKit) was retired across the ADR-008/011 line after an I2C
> slave-initialisation failure on the ESP32 platform (`i2c_new_slave_device()` triggering an
> unrecoverable watchdog reset; Espressif does not officially validate that slave driver path
> on the plain ESP32). The Raspberry Pi 3B+ replaced it directly at the same bus address.
> The entire shared-bus Database Controller role is itself now retired by ADR-012.

All devices shared a single I2C bus (400 kHz) and communicated via JSON-encoded messages
with a strict schema. Each device had its own OLED display showing real-time subsystem
state.

### Per-device bus architecture

Each ESP32-C3 MCU ran two independent I2C buses:
- **Shared bus (GPIO8/GPIO9):** inter-device communication via hardware `TwoWire(0)`.
- **Private OLED bus (GPIO3/GPIO10):** local display via U8g2 software I2C.

The ESP32-C3 has only one hardware I2C peripheral. That constraint was discovered during
development and resolved with U8g2 software I2C (bit-banged I2C in software on arbitrary
GPIO pins) — documented in ADR-002. **This OLED split is the one part of the I2C design that
survives the retirement.**

The Raspberry Pi 3B+ used a different mechanism for the same split:
- **Shared bus (GPIO18/GPIO19):** BCM2837 BSC slave peripheral — hardware-backed I2C slave,
  controlled via `pigpio bsc_i2c()`. GPIO18/19 are hardwired to this peripheral in silicon
  and cannot be reassigned. **(Retired by ADR-012 — see [Why it was retired](#-why-it-was-retired).)**
- **Private OLED (GPIO2/GPIO3):** BCM2837 BSC1 master peripheral via `luma.oled`. Separate
  hardware from the shared bus — no interference.

The RPi operated as a **pure I2C slave** on the shared bus: it never initiated a
transmission, and responses were returned via master-read (the requesting MCU stayed master
for the full exchange and clocked the response out of the RPi's TX FIFO). This is exactly
the path the 16-byte BSC FIFO undermined.

### FreeRTOS task architecture (MCUs) — *carries forward*

Each ESP32-C3 MCU ran a set of FreeRTOS tasks rather than a single Arduino `loop()`. Tasks
communicated via queues and shared resources via mutexes (mutual-exclusion locks). This was
a mid-project pivot (Phase 2.5) driven by a hardware constraint: `TwoWire` cannot switch
between master and slave mode at runtime without FreeRTOS managing the transition (ADR-007).

Each MCU ran at minimum:
- **Receiver task** (priority 3): wakes on I2C interrupt, puts messages on the inbound queue.
- **Logic task** (priority 2): drains the queue, handles messages, sends responses.
- **OLED task** (priority 1): updates the display independently at 500 ms intervals.

Subsystem-specific tasks were added per MCU role (HTTP server task on MCU #5, etc.). The task
pattern itself is sound and migrates to the successor.

### Python thread architecture (RPi)

The Raspberry Pi ran a Python service of three concurrent threads, mirroring the FreeRTOS
task + queue pattern:
- **bus_worker:** processed incoming bus messages, handled DB queries, loaded the TX FIFO.
- **oled_worker:** updated the SSD1306 display at 500 ms intervals.
- **flask thread** (planned Phase 4+): would serve transaction history over HTTP at port 5000.

See `docs/design/raspi_architecture.md` for the full (now historical) thread design.

---

## 💬 Example transaction flow (as designed)

A deposit of $100.00 into account 12345678, over the retired shared bus:

```
MCU #5  →  MCU #4    JOB_SUBMIT    { txnType: DEPOSIT, account: 12345678, amount: 10000 }
MCU #4  →  MCU #2    JOB_DISPATCH  { jobId: ..., txnType: DEPOSIT, account: 12345678, amount: 10000 }
MCU #2  →  RPi       DB_READ       { account: 12345678 }
RPi     →  MCU #2    DB_READ_RESULT { account: 12345678, balance: 50000, status: OK }
MCU #2  →  RPi       DB_WRITE      { account: 12345678, amount: 10000, newBalance: 60000 }
RPi     →  MCU #2    DB_WRITE_ACK  { account: 12345678, status: OK }
MCU #2  →  MCU #4    JOB_COMPLETE  { jobId: ..., status: SUCCESS }
MCU #4  →  MCU #5    JOB_RESULT    { jobId: ..., status: SUCCESS }
```

The RPi at address `0x0A` received `DB_READ`/`DB_WRITE` via I2C write transactions, then
returned results via I2C master-read (MCU #2 polled until the ready byte was set). All
monetary values are integer cents — `10000` = $100.00. Float arithmetic is never used for
money. **These message semantics carry forward to the successor; only the transport beneath
them changes.**

---

## 🛠️ Hardware (all carries forward)

| Component | Qty | Notes |
|-----------|-----|-------|
| ESP32-C3 SuperMini | 4 | USB-C, 4 MB flash, built-in WiFi/BLE. MCUs #1, #2, #4, #5 |
| Raspberry Pi 3B+ | 1 | Was Database Controller (BSC slave on shared bus); becomes central complex in the successor |
| 0.96" SSD1306 OLED (I2C) | 5 | One per device, 128×64 px |
| DS1307 RTC module | 1 | Master Console (MCU #1), transaction timestamps |
| Logic analyzer 8-ch 24 MHz | 1 | I2C bus debugging with PulseView/Sigrok |
| 5 kΩ pull-up resistors | 2 | SDA and SCL on the (now retired) shared bus |
| Breadboards + jumper wires | — | T-shape layout on 30×30 cm wood base |

> ESP32-WROOM-32 DevKit (38-pin, CP2102) — retired as MCU #3 across the ADR-008/011 line, and
> stays retired under ADR-012.

---

## 🛠️ Development environment (as used)

- **OS:** Fedora Linux
- **IDE:** VS Code with the PlatformIO extension
- **MCU language:** C++ (Arduino framework on ESP-IDF)
- **RPi language:** Python 3 (pigpio, luma.oled; Flask + sqlite3 were planned for Phase 4)
- **RTOS:** FreeRTOS (bundled with the ESP32 Arduino framework)
- **Key libraries:** Wire (I2C), U8g2 (software I2C OLED), ArduinoJson, RTClib
- **Debugging:** 8-channel 24 MHz logic analyzer + PulseView/Sigrok

---

## 📁 Repository structure (frozen)

```
code/
  shared/
    config/shared_config.h       ← pins, addresses, FreeRTOS stack sizes
    libs/oled_display/           ← U8g2-based OLED wrapper (all MCUs) — OLED path survives
    libs/shared_bus/             ← INACTIVE: I2C bus abstraction (preserved as record)
    libs/shared_bus_wroom/       ← INACTIVE: WROOM mode-switching lib (preserved as record)
    libs/message_protocol/       ← JSON envelope, schema validation, constants — carries forward
  mcu1-master-console/
  mcu2-transaction-processor/
  mcu3-database-controller/      ← retired firmware, preserved for reference
  mcu4-job-scheduler/
  mcu5-io-controller/
  raspi-db-server/               ← includes bus_slave.py (INACTIVE BSC slave code, preserved)
docs/
  requirements.md                ← functional and non-functional requirements
  devlog.md                      ← chronological build log with key learnings
  design/
    system_design.md             ← architecture, data flows, data model
    message_protocol.md          ← full JSON message format specification
    freertos_architecture.md     ← FreeRTOS task design for all MCUs
    raspi_architecture.md        ← Raspberry Pi thread architecture
  decisions/                     ← ADR-001 … ADR-012 (ADR-012 is terminal)
  poc_rtos/                      ← FreeRTOS PoC plan, results, sequence diagrams
  captures/                      ← PulseView captures, OLED photos
roadmap.md                       ← phased development plan (superseded by the successor)
scripts/
  claude_memory_sync.py          ← git hook: syncs CLAUDE.md to AI assistant memory
```

---

## 🗺️ Project status (final)

| Phase | Description | Status |
|---|---|---|
| 1 | Foundation — environment, I2C basics, OLED | ✅ Complete |
| 1.5 | Hardware fix — dual I2C bus, shared libraries | ✅ Complete |
| 2 | Protocol — JSON messaging, all devices on shared bus | ✅ Complete |
| 2.5 | FreeRTOS pivot — validated multi-master I2C with tasks | ✅ Complete |
| 3 | Individual subsystems — simple implementations per device | ⛔ In progress when retired — the Phase 3.3 BSC-FIFO finding triggered ADR-012 |
| 4 | Integration — production implementations, end-to-end flow | ⏭️ Not started — carried to successor |
| 5 | Advanced features — two-phase commit, crash recovery, load testing | ⏭️ Not started — carried to successor |

**Repository frozen at ADR-012.** New work begins in the successor project.

---

## 🔑 Key engineering decisions (record)

These decisions stand as the reasoning trail. The ones marked *carries forward* migrate to
the successor; the shared-bus-specific ones are now historical.

**FreeRTOS over Arduino `loop()`** *(carries forward)* — The ESP32-C3 `TwoWire` peripheral
locks into master or slave mode at boot, so slave MCUs could not initiate transmissions
without routing everything through MCU #1. FreeRTOS tasks with a mutex-protected bus let any
MCU temporarily become master. See ADR-007.

**Software I2C for OLED** *(survives)* — The ESP32-C3 has only one hardware I2C peripheral,
so OLED and shared bus could not both be hardware buses. U8g2 software I2C on separate GPIO
pins solved it cleanly. See ADR-002.

**Raspberry Pi as Database Controller** *(retired)* — The plain ESP32 I2C slave driver caused
an unrecoverable watchdog reset on boot, so the RPi's BCM2837 BSC slave peripheral was
adopted as a hardware-backed, better-supported alternative. This is the path ADR-012 retires:
the BSC slave's 16-byte FIFO made reliable reception a timing gamble. See ADR-010, ADR-011,
and the terminal ADR-012.

**Integer cents for money** *(carries forward)* — All monetary values are stored and
transmitted as `uint32_t` cents (`10000` = $100.00). Float arithmetic is never used for money
— a standard rule in financial software. See ADR-005.

**Strict JSON schema validation** *(carries forward)* — Every received message was validated
against a schema registry before processing; missing fields were rejected with a typed error
code. This caught several bugs that would otherwise have caused silent incorrect behaviour.

---

## 📚 Learning resources

- [IBM Redbooks — Introduction to the New Mainframe: z/OS Basics](https://www.redbooks.ibm.com/)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [ArduinoJson Documentation](https://arduinojson.org/)
- [U8g2 Library](https://github.com/olikraus/u8g2)
- [PulseView / Sigrok — I2C Decoder Guide](https://sigrok.org/wiki/Protocol_decoder:I2c)
- [pigpio library — bsc_i2c](https://abyz.me.uk/rpi/pigpio/python.html#bsc_i2c)

---

## 📝 License

This project is open source and available under the [MIT License](LICENSE).

---

*Built with curiosity, a soldering iron, and an unhealthy interest in how mainframes actually
work — and retired, on purpose, the moment the architecture (not the effort) stopped being
right.*
