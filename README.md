# 🖥️ Low-Level Mainframe Simulation Using MCUs

> Simulating a banking mainframe architecture using four ESP32-C3 microcontrollers and a
> Raspberry Pi 3B+ — because understanding distributed systems is way more fun when you
> can hold the hardware in your hands.

---

## 📖 Description

This project implements a low-level simulation of a banking mainframe using four ESP32-C3
microcontrollers and a Raspberry Pi 3B+, each representing a distinct mainframe subsystem.
The devices communicate over a shared I2C bus, mirroring the channel subsystem architecture
found in IBM z/OS mainframes.

Each subsystem operates autonomously while coordinating with its peers to process banking
transactions — deposits, withdrawals, transfers, and balance queries. The system is
intentionally low-level: no frameworks, no abstractions beyond what the project builds
itself. Every design decision is made visible and documented.

The project is educational in intent but rigorous in execution. Real hardware constraints
were discovered through testing and resolved through principled architectural decisions.
The devlog and ADRs document every significant finding.

---

## 🎯 Goals

### Primary — Mainframe Architecture
- Understand how real mainframes separate concerns across specialised hardware
- Learn job scheduling, I/O channel management, and transaction processing
- Observe distributed storage management and subsystem coordination in practice
- Experience failure modes, timeout handling, and health monitoring on real hardware

### Secondary — Embedded Systems
- Implement FreeRTOS multi-task architecture on resource-constrained hardware
- Design and validate a custom I2C multi-master bus protocol from scratch
- Build shared libraries used identically across four independent firmware images
- Debug hardware communication using a logic analyzer (PulseView/Sigrok)

---

## 🏗️ Architecture

Four ESP32-C3 microcontrollers and one Raspberry Pi 3B+, each mapped to a real mainframe
subsystem:

| MCU | Role | Mainframe Equivalent | I2C Address |
|-----|------|---------------------|-------------|
| #1 | Master Console | System Console / Operator Interface | `0x08` |
| #2 | Transaction Processor | Central Processor (CP) | `0x09` |
| RPi 3B+ | Database Controller | DASD Controller | `0x0A` |
| #4 | Job Scheduler | JES (Job Entry Subsystem) | `0x0B` |
| #5 | I/O Controller | Channel Subsystem | `0x0C` |

> MCU #3 (ESP32-WROOM-32 DevKit) was retired after exhaustive investigation into an
> I2C slave initialisation failure on the ESP32 platform. See ADR-011 for the full
> reasoning. The Raspberry Pi 3B+ replaces it directly at the same bus address.

All devices share a single I2C bus (400kHz) and communicate via JSON-encoded messages
with a strict schema. Each device has its own OLED display showing real-time subsystem
state.

### Per-device bus architecture

Each ESP32-C3 MCU runs two independent I2C buses:
- **Shared bus (GPIO8/GPIO9):** inter-device communication via hardware TwoWire(0)
- **Private OLED bus (GPIO3/GPIO10):** local display via U8g2 software I2C

The ESP32-C3 has only one hardware I2C peripheral. The constraint was discovered during
development and resolved with U8g2 software I2C — documented in ADR-002.

The Raspberry Pi 3B+ uses a different mechanism for the same split:
- **Shared bus (GPIO18/GPIO19):** BCM2837 BSC slave peripheral — hardware-backed I2C
  slave, controlled via `pigpio bsc_i2c()`. GPIO18/19 are hardwired to this peripheral
  in silicon and cannot be reassigned.
- **Private OLED (GPIO2/GPIO3):** BCM2837 BSC1 master peripheral via `luma.oled`.
  Separate hardware from the shared bus — no interference.

The RPi operates as a **pure I2C slave** on the shared bus. It never initiates a
transmission. Responses are returned via master-read: the requesting MCU stays master
for the full exchange and clocks the response out of the RPi's TX FIFO. See ADR-011
for the protocol details.

### FreeRTOS task architecture (MCUs)

Each ESP32-C3 MCU runs a set of FreeRTOS tasks rather than a single Arduino loop().
Tasks communicate via queues and share resources via mutexes. This was a mid-project
architectural pivot (Phase 2.5) driven by a hardware constraint: TwoWire cannot switch
between master and slave mode at runtime without FreeRTOS managing the transition.
The full reasoning is in ADR-007.

Each MCU runs at minimum:
- **Receiver task** (priority 3): wakes on I2C interrupt, puts messages on inbound queue
- **Logic task** (priority 2): drains queue, handles messages, sends responses
- **OLED task** (priority 1): updates display independently at 500ms intervals

Subsystem-specific tasks are added per MCU role (HTTP server task on MCU #5, etc.).

### Python thread architecture (RPi)

The Raspberry Pi runs a Python service structured as three concurrent threads, mirroring
the FreeRTOS task + queue pattern used on the MCUs:
- **bus_worker:** processes incoming bus messages, handles DB queries, loads TX FIFO
- **oled_worker:** updates the SSD1306 display at 500ms intervals
- **flask thread** (Phase 4+): serves transaction history over HTTP at port 5000

See `docs/design/raspi_architecture.md` for the full thread design.

---

## 💬 Example Transaction Flow

A deposit of $100.00 into account 12345678:

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

The RPi at address 0x0A receives DB_READ and DB_WRITE via I2C write transactions, then
returns results via I2C master-read (MCU #2 polls until the ready byte is set). All
monetary values are integer cents — `10000` = $100.00. Float arithmetic is never used
for money.

---

## 🛠️ Hardware

| Component | Qty | Notes |
|-----------|-----|-------|
| ESP32-C3 SuperMini | 4 | USB-C, 4MB flash, built-in WiFi/BLE. MCUs #1, #2, #4, #5 |
| Raspberry Pi 3B+ | 1 | Database Controller — I2C slave on shared bus via GPIO18/19 (BSC slave) |
| 0.96" SSD1306 OLED (I2C) | 5 | One per device, 128×64px |
| DS1307 RTC module | 1 | Master Console (MCU #1), transaction timestamps |
| Logic analyzer 8-ch 24MHz | 1 | I2C bus debugging with PulseView/Sigrok |
| 5kΩ pull-up resistors | 2 | SDA and SCL lines on shared bus |
| Breadboards + jumper wires | — | T-shape layout on 30×30cm wood base |

> ESP32-WROOM-32 DevKit (38-pin, CP2102) — retired as MCU #3. See ADR-011.

---

## 🛠️ Development Environment

- **OS:** Fedora Linux
- **IDE:** VS Code with PlatformIO extension
- **MCU language:** C++ (Arduino framework on ESP-IDF)
- **RPi language:** Python 3 (pigpio, luma.oled; Flask + sqlite3 in Phase 4)
- **RTOS:** FreeRTOS (bundled with ESP32 Arduino framework)
- **Key Libraries:** Wire (I2C), U8g2 (software I2C OLED), ArduinoJson, RTClib
- **Debugging:** 8-channel 24MHz logic analyzer + PulseView/Sigrok

---

## 📁 Repository Structure

```
code/
  shared/
    config/shared_config.h       ← pins, addresses, FreeRTOS stack sizes
    libs/oled_display/           ← U8g2-based OLED wrapper (all MCUs)
    libs/shared_bus/             ← I2C bus abstraction, FreeRTOS task-safe
    libs/message_protocol/       ← JSON envelope, schema validation, constants
  mcu1-master-console/
  mcu2-transaction-processor/
  mcu3-database-controller/      ← retired firmware, preserved for reference
  mcu4-job-scheduler/
  mcu5-io-controller/
  raspi-db-server/               ← Raspberry Pi Database Controller service
docs/
  requirements.md                ← functional and non-functional requirements
  devlog.md                      ← chronological build log with key learnings
  design/
    system_design.md             ← architecture, data flows, data model
    message_protocol.md          ← full JSON message format specification
    freertos_architecture.md     ← FreeRTOS task design for all MCUs
    raspi_architecture.md        ← Raspberry Pi thread architecture
  decisions/                     ← Architecture Decision Records (ADR-001–011)
  poc_rtos/                      ← FreeRTOS PoC plan, results, sequence diagrams
  captures/                      ← PulseView captures, OLED photos
roadmap.md                       ← phased development plan
scripts/
  claude_memory_sync.py          ← git hook: syncs CLAUDE.md to AI assistant memory
```

---

## 🗺️ Project Status

| Phase | Description | Status |
|---|---|---|
| 1 | Foundation — environment, I2C basics, OLED | ✅ Complete |
| 1.5 | Hardware fix — dual I2C bus, shared libraries | ✅ Complete |
| 2 | Protocol — JSON messaging, all devices on shared bus | ✅ Complete |
| 2.5 | FreeRTOS pivot — validated multi-master I2C with tasks | ✅ Complete |
| 3 | Individual subsystems — simple implementations per device | 🔄 In progress |
| 4 | Integration — production implementations, end-to-end flow | ⏳ Planned |
| 5 | Advanced features — two-phase commit, crash recovery, load testing | ⏳ Planned |

---

## 📚 Documentation

- [Roadmap](roadmap.md)
- [Requirements](docs/requirements.md)
- [System Design](docs/design/system_design.md)
- [FreeRTOS Architecture](docs/design/freertos_architecture.md)
- [Raspberry Pi Architecture](docs/design/raspi_architecture.md)
- [Message Protocol](docs/design/message_protocol.md)
- [Architecture Decisions](docs/decisions/)
- [Dev Log](docs/devlog.md)

---

## 🔑 Key Engineering Decisions

**FreeRTOS over Arduino loop()** — The ESP32-C3 TwoWire peripheral locks into master or
slave mode at boot. Slave MCUs cannot initiate transmissions, which prevents
subsystem-to-subsystem communication without routing everything through MCU #1. FreeRTOS
tasks with a mutex-protected bus allow any MCU to temporarily become master. See ADR-007.

**Software I2C for OLED** — The ESP32-C3 has only one hardware I2C peripheral. Running
OLED and shared bus on separate hardware buses is impossible. U8g2 software I2C on
separate GPIO pins solves this cleanly. See ADR-002.

**Raspberry Pi as Database Controller** — After exhaustive investigation, the plain ESP32
I2C slave driver (`i2c_new_slave_device()`) caused an unrecoverable watchdog reset on every
boot. Espressif does not officially validate this driver path on the plain ESP32. The RPi's
BCM2837 BSC slave peripheral is hardware-backed, well-supported via pigpio, and provides a
far more transparent debugging environment. See ADR-011.

**Integer cents for money** — All monetary values are stored and transmitted as `uint32_t`
cents. `10000` = $100.00. Float arithmetic is never used for money — a standard rule in
financial software. See ADR-005.

**Strict JSON schema validation** — Every received message is validated against a schema
registry before processing. Missing fields are rejected with a typed error code. This
caught several bugs during development that would otherwise have caused silent incorrect
behaviour.

---

## 📚 Learning Resources

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

*Built with curiosity, a soldering iron, and an unhealthy interest in how mainframes actually work.*