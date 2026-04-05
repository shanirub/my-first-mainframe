# ADR-001: I2C as inter-MCU communication protocol

## Status
Accepted

## Context
Five MCUs need to communicate with each other. Protocol options considered:
- I2C
- SPI
- UART (one-to-one only)
- CAN bus

## Decision
Use I2C shared bus at 400kHz Fast Mode for all inter-MCU communication.

## Reasoning
- Multi-master capable — any MCU can initiate communication
- Only 2 signal wires (SDA + SCL) shared across all 5 MCUs
- Built-in addressing — each MCU gets its own address (0x08–0x0C)
- Sufficient speed for JSON messages at our scale
- Native Arduino Wire library support on ESP32
- Teaches real mainframe channel subsystem concepts (bus arbitration, addressing)

## Consequences
- Default I2C buffer is 128 bytes — expanded to 256 via build flag (see ADR-004)
- Only one hardware I2C peripheral on ESP32-C3 (see ADR-002)
- Pull-up resistors required on SDA and SCL lines (5kΩ confirmed working)
- Bus is sequential — only one transmission at a time
- All MCUs must share a common GND reference
