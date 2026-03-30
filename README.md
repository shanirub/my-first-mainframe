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
- **Private bus (GPIO3/GPIO10):** connects to its own OLED display
- **Shared bus (GPIO8/GPIO9):** inter-MCU communication

---

## 🛠️ Hardware

The project is built around five ESP32-C3 SuperMini microcontrollers — compact, USB-C capable boards with built-in WiFi and Bluetooth. Each MCU sits on its own breadboard alongside a 0.96" SSD1306 OLED display for real-time status output.

Storage for the Database Controller subsystem is provided by a Micro SD card module connected via SPI. An RTC module (DS1307) on the Master Console provides accurate timestamps for transaction logs.

A USB splitter cable distributes power to all five MCUs from a single bench power supply, keeping the wiring clean. An 8-channel 24MHz logic analyzer connects to the shared I2C bus for signal inspection and debugging with PulseView.

Prototyping is done on solderless breadboards with jumper wires. Pull-up resistors (4.7kΩ) are required on the shared I2C bus lines.

---

## 🛠️ Development Environment

- **OS:** Fedora Linux
- **IDE:** VS Code with PlatformIO extension
- **Language:** C++ (Arduino framework)
- **Key Libraries:** Wire (I2C), ArduinoJson, Adafruit SSD1306, RTClib, SD
- **Debugging:** 8-channel 24MHz Logic Analyzer + PulseView/Sigrok

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

*Built with curiosity, a soldering iron, and an unhealthy excitement about distributed systems.*
