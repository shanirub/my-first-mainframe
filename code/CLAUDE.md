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
Each MCU runs two independent I2C buses:
- Bus 0 (shared inter-MCU): GPIO8=SDA, GPIO9=SCL, 400kHz
- Bus 1 (private OLED):     GPIO3=SDA, GPIO10=SCL, 100kHz

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
    libs/oled_display/     ← shared OLED library (Adafruit SSD1306 wrapper)
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
See roadmap.md for phase progress.
Task 3 COMPLETE: bidirectional I2C communication confirmed between MCU #1 
and MCU #2 on shared bus (GPIO8/GPIO9). Serial monitor verified both sides.

Key fix discovered: TwoWire.begin() slave overload requires explicit frequency 
argument — begin(I2C_ADDRESS, SHARED_SDA_PIN, SHARED_SCL_PIN, 0). 
Without the 0, it silently hits wrong overload and ignores pin assignments.

Next: PulseView capture of live transmission, then merge OLED code back 
into both MCUs (shared bus + private OLED bus running simultaneously).