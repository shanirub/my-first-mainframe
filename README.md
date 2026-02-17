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

---

## 🗺️ Planned Roadmap

### Phase 1 — Foundation *(Week 1)*
- Development environment setup (Arduino IDE + ESP32 board support)
- Flash and verify all MCUs individually
- Test OLED displays per subsystem
- Establish basic I2C communication between two MCUs

### Phase 2 — Protocol *(Week 2)*
- Implement JSON message format using ArduinoJson
- Connect all 5 MCUs to shared I2C bus
- Test multi-MCU message broadcasting and addressing

### Phase 3 — Individual Subsystems *(Weeks 3–4)*
- Build Master Console: serial input + WiFi web dashboard
- Build Database Controller: SD card read/write with JSON storage
- Build Transaction Processor: validation logic and routing
- Build Job Scheduler: priority queue implementation
- Build I/O Controller: transaction generation and result handling
- Test each subsystem independently before integration

### Phase 4 — Integration *(Week 5)*
- Connect all subsystems and run end-to-end transaction flow
- Implement and test full banking scenarios (deposit, withdrawal, transfer)
- Validate heartbeat and health monitoring across subsystems

### Phase 5 — Advanced Features *(Week 6)*
- Priority job scheduling with HIGH / MEDIUM / LOW queues
- Atomic transfer transactions (all-or-nothing semantics)
- Load testing (50 concurrent transactions)
- Failure simulation (physical subsystem disconnection)
- Logic analyzer debugging sessions on I2C bus traffic

---

## 🧪 Test Scenarios

Six concrete scenarios drive development and validation:

1. **Simple Deposit** — Basic transaction flow end-to-end
2. **Insufficient Funds** — Error propagation and rollback
3. **Account Transfer** — Atomic multi-account operation
4. **Priority Scheduling** — HIGH priority job preempting LOW priority queue
5. **Load Test** — 50 rapid transactions, observe bottlenecks
6. **Subsystem Failure** — Disconnect DB Controller, observe timeout and recovery

---

## 🛒 Hardware

| Component | Quantity | Purpose |
|-----------|----------|---------|
| ESP32-C3 SuperMini | 6 (1 spare) | Subsystem MCUs |
| SSD1306 OLED 0.96" | 5 | Per-subsystem status display |
| Micro SD Card Module (SPI) | 2 (1 spare) | Database Controller storage |
| 64GB Micro SD Card | 1 | Account data persistence |
| USB Logic Analyzer 8CH 24MHz | 1 | I2C bus debugging |
| Breadboards (830pt × 2, 400pt × 2) | 4 | Prototyping platform |
| Jumper wire kit | 1 | Connections |
| USB-A to 6× USB-C splitter | 1 | Power distribution |

**Existing equipment used:**
- Wanptek WPS3010H bench power supply (30V/10A) — USB output powers all MCUs
- Elegoo DS1307 RTC module — Timestamps on Master Console
- Assorted LEDs and resistors (including 4.7kΩ for I2C pull-ups)

**Total hardware cost:** ~$104 including international shipping

---

## 📦 Repository Contents

```
/
├── README.md                        # This file
├── design/
│   ├── mainframe_simulation_design.docx   # Full system design document
│   └── wiring_schematic.html              # Interactive wiring schematic
```

Code will be added per subsystem as development progresses:

```
├── firmware/
│   ├── master_console/
│   ├── transaction_processor/
│   ├── database_controller/
│   ├── job_scheduler/
│   └── io_controller/
└── docs/
    └── learnings.md                 # Notes and insights along the way
```

---

## 📍 Current Status

> **⏳ Waiting for parts to arrive!**

Hardware has been ordered and is on its way. In the meantime, the system design is complete, wiring schematics are finalized, and the development roadmap is ready to go.

The excitement is real. Parts can't arrive fast enough. 🎉

---

## 🛠️ Development Environment

- **OS:** Fedora Linux
- **IDE:** Arduino IDE with ESP32-C3 board support
- **Language:** C++ (Arduino framework)
- **Key Libraries:** Wire (I2C), ArduinoJson, Adafruit SSD1306, RTClib, SD
- **Debugging:** HiBCTR 24MHz 8-Channel Logic Analyzer + PulseView

---

## 📚 Learning Resources

- [IBM Redbooks — Introduction to the New Mainframe: z/OS Basics](https://www.redbooks.ibm.com/)
- [Random Nerd Tutorials — ESP32 I2C Communication](https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/)
- [ArduinoJson Documentation](https://arduinojson.org/)
- [PulseView / Sigrok — I2C Decoder Guide](https://sigrok.org/wiki/Protocol_decoder:I2c)

---

## 📝 License

This project is open source and available under the [MIT License](LICENSE).

---

*Built with curiosity, a soldering iron (not yet needed), and an unhealthy excitement about distributed systems.*
