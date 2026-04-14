# 🖥️ Low-Level Mainframe Simulation Using MCUs

> Simulating a banking mainframe architecture using five ESP32-C3 microcontrollers — because understanding distributed systems is way more fun when you can hold the hardware in your hands.

---

## 📖 Description

This project implements a low-level simulation of a banking mainframe using five ESP32-C3 microcontrollers, each representing a distinct mainframe subsystem. The MCUs communicate over a shared I2C bus, mirroring the channel subsystem architecture found in IBM z/OS mainframes.

Each subsystem operates autonomously while coordinating with its peers to process banking transactions — deposits, withdrawals, transfers, and balance queries. The system is intentionally low-level: no frameworks, no abstractions beyond what the project builds itself. Every design decision is made visible and documented.

The project is educational in intent but rigorous in execution. Real hardware constraints were discovered through testing and resolved through principled architectural decisions. The devlog and ADRs document every significant finding.

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
- Build shared libraries used identically across five independent firmware images
- Debug hardware communication using a logic analyzer (PulseView/Sigrok)

---

## 🏗️ Architecture

Five ESP32-C3 microcontrollers, each mapped to a real mainframe subsystem:

| MCU | Role | Mainframe Equivalent | I2C Address |
|-----|------|---------------------|-------------|
| #1 | Master Console | System Console / Operator Interface | `0x08` |
| #2 | Transaction Processor | Central Processor (CP) | `0x09` |
| #3 | Database Controller | DASD Controller | `0x0A` |
| #4 | Job Scheduler | JES (Job Entry Subsystem) | `0x0B` |
| #5 | I/O Controller | Channel Subsystem | `0x0C` |

All five MCUs share a single I2C bus (400kHz) and communicate via JSON-encoded messages with a strict schema. Each MCU has its own OLED display showing real-time subsystem state.

### Per-MCU bus architecture

Each MCU runs two independent I2C buses:
- **Shared bus (GPIO8/GPIO9):** inter-MCU communication via hardware TwoWire(0)
- **Private OLED bus (GPIO3/GPIO10):** local display via U8g2 software I2C

This split was necessary because the ESP32-C3 has only one hardware I2C peripheral. The constraint was discovered during development and resolved with U8g2 software I2C — documented in ADR-002.

### FreeRTOS task architecture

Each MCU runs a set of FreeRTOS tasks rather than a single Arduino loop(). Tasks communicate via queues and share resources via mutexes. This was a mid-project architectural pivot (Phase 2.5) driven by a hardware constraint: TwoWire cannot switch between master and slave mode at runtime without FreeRTOS managing the transition. The full reasoning is in ADR-007.

Each MCU runs at minimum:
- **Receiver task** (priority 3): wakes on I2C interrupt, puts messages on inbound queue
- **Logic task** (priority 2): drains queue, handles messages, sends responses
- **OLED task** (priority 1): updates display independently at 500ms intervals

Subsystem-specific tasks are added per MCU role (SD card task on MCU #3, HTTP server task on MCU #5, etc.).

---

## 💬 Example Transaction Flow

A deposit of $100.00 into account 12345678:

```
MCU #5  →  MCU #4   JOB_SUBMIT    { txnType: DEPOSIT, account: 12345678, amount: 10000 }
MCU #4  →  MCU #2   JOB_DISPATCH  { jobId: ..., txnType: DEPOSIT, account: 12345678, amount: 10000 }
MCU #2  →  MCU #3   DB_READ       { account: 12345678 }
MCU #3  →  MCU #2   DB_READ_RESULT { account: 12345678, balance: 50000, status: OK }
MCU #2  →  MCU #3   DB_WRITE      { account: 12345678, amount: 10000, newBalance: 60000 }
MCU #3  →  MCU #2   DB_WRITE_ACK  { account: 12345678, status: OK }
MCU #2  →  MCU #4   JOB_COMPLETE  { jobId: ..., status: SUCCESS }
MCU #4  →  MCU #5   JOB_RESULT    { jobId: ..., status: SUCCESS }
```

All monetary values are integer cents — `10000` = $100.00. Float arithmetic is never used for money.

---

## 🛠️ Hardware

| Component | Qty | Notes |
|-----------|-----|-------|
| ESP32-C3 SuperMini | 5 | USB-C, 4MB flash, built-in WiFi/BLE |
| 0.96" SSD1306 OLED (I2C) | 5 | One per MCU, 128×64px |
| Micro SD card module (SPI) | 1 | Database Controller (MCU #3) |
| DS1307 RTC module | 1 | Master Console (MCU #1), transaction timestamps |
| Logic analyzer 8-ch 24MHz | 1 | I2C bus debugging with PulseView/Sigrok |
| 5kΩ pull-up resistors | 2 | SDA and SCL lines on shared bus |
| Breadboards + jumper wires | — | T-shape layout on 30×30cm wood base |

---

## 🛠️ Development Environment

- **OS:** Fedora Linux
- **IDE:** VS Code with PlatformIO extension
- **Language:** C++ (Arduino framework on ESP-IDF)
- **RTOS:** FreeRTOS (bundled with ESP32 Arduino framework)
- **Key Libraries:** Wire (I2C), U8g2 (software I2C OLED), ArduinoJson, RTClib, SD
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
  mcu3-database-controller/
  mcu4-job-scheduler/
  mcu5-io-controller/
docs/
  requirements.md                ← functional and non-functional requirements
  devlog.md                      ← chronological build log with key learnings
  design/
    system_design.md             ← architecture, data flows, data model
    message_protocol.md          ← full JSON message format specification
    freertos_architecture.md     ← FreeRTOS task design for all 5 MCUs
  decisions/                     ← Architecture Decision Records (ADR-001–007)
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
| 2 | Protocol — JSON messaging, all 5 MCUs on shared bus | ✅ Complete |
| 2.5 | FreeRTOS pivot — validated multi-master I2C with tasks | ✅ Complete |
| 3 | Individual subsystems — simple implementations per MCU | 🔄 In progress |
| 4 | Integration — production implementations, end-to-end flow | ⏳ Planned |
| 5 | Advanced features — two-phase commit, crash recovery, load testing | ⏳ Planned |

---

## 📚 Documentation

- [Roadmap](roadmap.md)
- [Requirements](docs/requirements.md)
- [System Design](docs/design/system_design.md)
- [FreeRTOS Architecture](docs/design/freertos_architecture.md)
- [Message Protocol](docs/design/message_protocol.md)
- [Architecture Decisions](docs/decisions/)
- [Dev Log](docs/devlog.md)

---

## 🔑 Key Engineering Decisions

**FreeRTOS over Arduino loop()** — The ESP32-C3 TwoWire peripheral locks into master or slave mode at boot. Slave MCUs cannot initiate transmissions, which prevents subsystem-to-subsystem communication without routing everything through MCU #1. FreeRTOS tasks with a mutex-protected bus allow any MCU to temporarily become master. See ADR-007.

**Software I2C for OLED** — The ESP32-C3 has only one hardware I2C peripheral. Running OLED and shared bus on separate hardware buses is impossible. U8g2 software I2C on separate GPIO pins solves this cleanly. See ADR-002.

**Integer cents for money** — All monetary values are stored and transmitted as `uint32_t` cents. `10000` = $100.00. Float arithmetic is never used for money — a standard rule in financial software. See ADR-005.

**Strict JSON schema validation** — Every received message is validated against a schema registry before processing. Missing fields are rejected with a typed error code. This caught several bugs during development that would otherwise have caused silent incorrect behaviour.

---

## 📚 Learning Resources

- [IBM Redbooks — Introduction to the New Mainframe: z/OS Basics](https://www.redbooks.ibm.com/)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [ArduinoJson Documentation](https://arduinojson.org/)
- [U8g2 Library](https://github.com/olikraus/u8g2)
- [PulseView / Sigrok — I2C Decoder Guide](https://sigrok.org/wiki/Protocol_decoder:I2c)

---

## 📝 License

This project is open source and available under the [MIT License](LICENSE).

---

*Built with curiosity, a soldering iron, and an unhealthy interest in how mainframes actually work.*
