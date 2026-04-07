# Mainframe Simulation — Claude Context

## Project Summary
Educational simulation of a banking mainframe using 5× ESP32-C3 SuperMini
MCUs. Each MCU represents a distinct subsystem communicating over a shared
I2C bus. Built on Fedora Linux with VS Code + PlatformIO.

## Hardware
- MCUs: 5× ESP32-C3 SuperMini
- Displays: 0.96" SSD1306 128×64 OLED (I2C), one per MCU
- Breadboards: 2× large (64-row), 2× small (30-row)
- Logic analyzer: 8-channel 24MHz (PulseView/Sigrok)
- Soldering station: YOUYUE 8586

## I2C Bus Architecture — Per MCU
Each MCU runs two I2C buses:
- Bus 0 (shared inter-MCU): GPIO8=SDA, GPIO9=SCL, 400kHz — hardware TwoWire(0)
- OLED (private):           GPIO3=SDA, GPIO10=SCL, 100kHz — U8g2 software I2C (bit-bang)

Note: ESP32-C3 has only one hardware I2C peripheral (SOC_I2C_NUM=1).
TwoWire(1) silently fails at runtime. OLED uses U8g2 SW_I2C to avoid the conflict.

## MCU Addressing
| MCU | Role                  | I2C Address |
|-----|-----------------------|-------------|
| #1  | Master Console        | 0x08        |
| #2  | Transaction Processor | 0x09        |
| #3  | Database Controller   | 0x0A        |
| #4  | Job Scheduler         | 0x0B        |
| #5  | I/O Controller        | 0x0C        |

## Project Structure
code/
  shared/
    libs/oled_display/     ← shared OLED library (U8g2 SW_I2C wrapper)
    libs/shared_bus/       ← shared I2C bus library (master + slave, poll() pattern)
    config/shared_config.h ← all pin definitions and MCU addresses
  mcu1-master-console/
  mcu2-transaction-processor/
  ...
Each MCU has: platformio.ini, src/main.cpp, src/config.h

## Shared Config Pattern
- shared_config.h defines all pins and addresses
- Each MCU's src/config.h includes shared_config.h and adds #define I2C_ADDRESS
- Use OLED_SDA_PIN / OLED_SCL_PIN / SHARED_SDA_PIN / SHARED_SCL_PIN everywhere
- Never use raw pin numbers in main.cpp

## Known Bugs / Confirmed Fixes
- ESP32-C3 Wire.begin() bug: never use Wire directly for non-default pins.
  Always use TwoWire sharedBus = TwoWire(0) with explicit pin assignment.
- ESP32-C3 has SOC_I2C_NUM=1: only one hardware I2C peripheral exists.
  TwoWire(1).begin() returns ESP_ERR_INVALID_ARG and leaves txBuffer NULL —
  no error is surfaced to Arduino code. Use U8g2 SW_I2C for the OLED bus.
- board_build.mcu = esp32c3 is mandatory in every platformio.ini alongside
  board = esp32-c3-devkitm-1. Without it builds fail silently.
- library.json is required in each shared library folder for PlatformIO
  to link it correctly.

## PlatformIO lib_extra_dirs
Each MCU's platformio.ini must include:
    lib_extra_dirs =
        ../shared/libs
        ../shared/config
    lib_ldf_mode = deep+

## Development Principles
- Baby-step progression: one variable at a time, explicit pass/fail criteria
- Serial monitor first, PulseView for deeper inspection
- Never touch code outside the current task scope
- Back up working main.cpp before replacing it for a new task

## Current Status
Phase 2 COMPLETE. See roadmap.md.

MCU #1 verified communicating with each of MCU #2, #3, #4, #5 individually
(sequential pair tests). Full 5-MCU simultaneous bus test not yet done —
this is the first task of Phase 3 before subsystem logic is implemented.

Shared libraries complete and verified:
- shared/libs/oled_display       — U8g2 software I2C OLED wrapper
- shared/libs/shared_bus         — I2C bus abstraction, poll() pattern
- shared/libs/message_protocol   — JSON envelope, validation, constants

Next: Phase 3 — connect all 5 to hub simultaneously, verify bus,
then implement individual subsystem logic.

## PulseView Setup (Logic Analyzer)
Physical connections for shared bus capture:
- D0 = SDA (GPIO8, blue wire)
- D1 = SCL (GPIO9, orange wire)
In PulseView I2C decoder: assign SCL→D1, SDA→D0 (opposite of channel defaults)

## Git Hooks
A post-commit hook automatically syncs CLAUDE.md changes to Claude's memory.
Hook location: .git/hooks/post-commit
Script location: scripts/claude_memory_sync.py
Trigger: only fires when CLAUDE.md files are modified in the commit.

