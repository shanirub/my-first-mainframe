# 🖥️ Low-Level Mainframe Simulation Using MCUs

> Simulating a banking mainframe architecture using five ESP32-C3 microcontrollers — because understanding distributed systems is way more fun when you can hold the hardware in your hands.

---

## 📖 Description

This project implements a low-level simulation of a mainframe banking system using five ESP32-C3 microcontrollers, each representing a distinct mainframe subsystem. The MCUs communicate over a shared I2C bus, mirroring the channel subsystem architecture found in IBM z/OS mainframes.

Each subsystem operates autonomously while coordinating with its peers to process banking transactions — deposits, withdrawals, transfers, and balance queries — providing a hands-on, tangible way to understand how large-scale distributed systems actually work under the hood.

The system is intentionally low-level and educational. This is not a production banking system. It is, however, a genuinely illuminating way to learn mainframe architecture concepts that most backend developers never get close to.

---

## 🎯 Goals

### Primary Goal — Mainframe Architecture
- Understand how real mainframes separate concerns across specialized hardware
- Learn job scheduling, I/O channel management, and transaction processing
- Observe distributed storage management and subsystem coordination in practice
- Experience failure modes, timeout handling, and health monitoring firsthand

### Secondary Goal — MCU Communication
- Implement I2C multi-master shared bus communication
- Learn bus arbitration, addressing, and collision handling
- Debug hardware communication using a logic analyzer (PulseView/Sigrok)
- Understand the tradeoffs between different inter-device communication protocols

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

All subsystems share a single I2C bus (400kHz Fast Mode) and communicate via JSON-encoded messages. Each MCU has its own OLED display showing real-time subsystem state.

Each MCU runs two independent I2C buses:
- **Shared bus (GPIO8/GPIO9):** inter-MCU communication (hardware I2C)
- **Private bus (GPIO3/GPIO10):** local OLED display (software I2C via U8g2)

---

## 💬 Example Transactions

**Deposit $100.00 into account 12345678:**
```json
{ "msgId": 1, "from": 12, "to": 11, "type": "JOB_SUBMIT",
  "payload": { "txnType": "DEPOSIT", "account": "12345678", "amount": 10000, "priority": "MEDIUM" }}
```

**Check account balance:**
```json
{ "msgId": 2, "from": 12, "to": 11, "type": "JOB_SUBMIT",
  "payload": { "txnType": "BALANCE", "account": "12345678", "priority": "LOW" }}
```

**Withdraw with insufficient funds — MCU #2 rejects:**
```json
{ "msgId": 3, "from": 11, "to": 12, "type": "JOB_RESULT",
  "payload": { "jobId": "0C03", "status": "FAILED", "reason": "INSUFFICIENT_FUNDS" }}
```

All monetary values are stored as integer cents — `10000` = $100.00. Float arithmetic is never used for money.

---

## 🛠️ Hardware

| Component | Qty | Notes |
|-----------|-----|-------|
| ESP32-C3 SuperMini | 5 | USB-C, 4MB flash, built-in WiFi |
| 0.96" SSD1306 OLED (I2C) | 5 | One per MCU, 128×64px |
| Micro SD card module (SPI) | 1 | Database Controller (MCU #3) |
| DS1307 RTC module | 1 | Master Console (MCU #1), timestamps |
| Logic analyzer 8-ch 24MHz | 1 | I2C bus debugging with PulseView |
| 5kΩ pull-up resistors | 2 | SDA and SCL lines on shared bus |
| Breadboards + jumper wires | — | T-shape layout on 30×30cm wood base |

---

## 🛠️ Development Environment

- **OS:** Fedora Linux
- **IDE:** VS Code with PlatformIO extension
- **Language:** C++ (Arduino framework)
- **Key Libraries:** Wire (I2C), U8g2 (software I2C OLED), ArduinoJson, RTClib, SD
- **Debugging:** 8-channel 24MHz logic analyzer + PulseView/Sigrok

---

## 📁 Repository Structure

```
code/
  shared/
    config/shared_config.h    ← all pin definitions and MCU addresses
    libs/oled_display/        ← U8g2-based OLED wrapper (shared by all MCUs)
    libs/shared_bus/          ← I2C bus abstraction with poll() pattern
  mcu1-master-console/
  mcu2-transaction-processor/
  mcu3-database-controller/
  mcu4-job-scheduler/
  mcu5-io-controller/
docs/
  requirements.md             ← functional and non-functional requirements
  design/
    system_design.md          ← architecture, data flows, data model
    message_protocol.md       ← full JSON message format specification
  decisions/                  ← Architecture Decision Records (ADR-001 to ADR-006)
  captures/                   ← PulseView captures and sequence diagrams
  devlog.md                   ← chronological build log
scripts/
  claude_memory_sync.py       ← git hook: syncs CLAUDE.md changes to Claude memory
```

---

## 📚 Documentation

- [Requirements](docs/requirements.md)
- [System Design](docs/design/system_design.md)
- [Message Protocol](docs/design/message_protocol.md)
- [Architecture Decisions](docs/decisions/)
- [Dev Log](docs/devlog.md)
- [Roadmap](roadmap.md)

---

## 📚 Learning Resources

- [IBM Redbooks — Introduction to the New Mainframe: z/OS Basics](https://www.redbooks.ibm.com/)
- [ArduinoJson Documentation](https://arduinojson.org/)
- [U8g2 Library](https://github.com/olikraus/u8g2)
- [PulseView / Sigrok — I2C Decoder Guide](https://sigrok.org/wiki/Protocol_decoder:I2c)

---

## 📝 License

This project is open source and available under the [MIT License](LICENSE).

---

*Built with curiosity, a soldering iron, and an unhealthy excitement about distributed systems.*
