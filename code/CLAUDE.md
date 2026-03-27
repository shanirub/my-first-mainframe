# Mainframe Simulation Project — Claude Code Context

## What This Project Is

An educational hardware simulation of a banking mainframe architecture,
using 5× ESP32-C3 SuperMini microcontrollers as distinct subsystems.
Each MCU represents one mainframe subsystem, communicates with the others
over a shared I2C bus, and has its own private OLED display.

This is a learning project. The primary goals are understanding mainframe
architecture and MCU-to-MCU communication via I2C.

---

## The Five Subsystems

| MCU | Role               | I2C Address | Notes                        |
|-----|--------------------|-------------|------------------------------|
| #1  | Master Console     | 0x08        | System coordinator, web dashboard |
| #2  | Transaction Processor | 0x09     | Validates and routes transactions |
| #3  | Database Controller | 0x0A       | Account data, SD card storage |
| #4  | Job Scheduler      | 0x0B        | Task queues and priorities   |
| #5  | I/O Controller     | 0x0C        | Simulates ATM/terminal input |

---

## Hardware: ESP32-C3 SuperMini

- **Onboard LED:** GPIO8 (active HIGH)
- **Shared inter-MCU I2C bus:** GPIO8 = SDA, GPIO9 = SCL  
  ⚠️ GPIO8 is shared with the onboard LED — avoid LED usage once I2C bus is active
- **Private OLED I2C bus:** GPIO0 = SDA, GPIO1 = SCL
- **USB-C** for programming and serial monitor
- **3.3V logic** throughout

### Critical platformio.ini requirement

Both of these lines are required — without `board_build.mcu`, the wrong
MCU variant is used and things silently break:

```ini
[env:esp32-c3-devkitm-1]
platform = espressif32
board = esp32-c3-devkitm-1
board_build.mcu = esp32c3
framework = arduino
monitor_speed = 115200
upload_port = /dev/ttyACM0
monitor_port = /dev/ttyACM0
```

---

## I2C Architecture: Two Buses Per MCU

Each MCU runs **two independent I2C buses simultaneously**:

### Bus 0 — Shared inter-MCU bus (Wire)
- **GPIO8** = SDA, **GPIO9** = SCL
- Shared by all 5 MCUs
- Used for MCU-to-MCU communication
- Addresses: 0x08 through 0x0C (one per MCU)
- Init: `Wire.begin(8, 9)` (or as slave: `Wire.begin(MY_ADDRESS, 8, 9)`)

### Bus 1 — Private OLED bus (TwoWire)
- **GPIO0** = SDA, **GPIO1** = SCL
- Private to each MCU — not shared
- OLED address: 0x3C
- Init: `TwoWire I2CBus1(1); I2CBus1.begin(0, 1);`
- Display init: `Adafruit_SSD1306 display(128, 64, &I2CBus1, -1);`

**Why two buses?** The OLED default address (0x3C) would conflict with
MCU addresses on the shared bus. Keeping OLEDs on a separate bus
eliminates that conflict entirely.

---

## OLED Display Status

- **Original batch of 5 OLEDs: confirmed defective.** All 5 units from
  the same manufacturer/batch never displayed anything across all
  code/library/wiring combinations. This is a hardware fault, not a
  code or wiring issue.
- **Replacements ordered:** 0.96" SSD1306 128×64 I2C (rectangular PCB,
  multiple AliExpress sellers) + LCD 1602 I2C as backup.
- **Project is paused** until replacement displays arrive.

### OLED purchasing checklist (for future reference)
- Driver: SSD1306 (not SH1106 or SH1107)
- Resolution: 128×64 (not 128×32)
- Interface: I2C with 4-pin header (VCC, GND, SDA, SCL)
- Size: 0.96"
- Voltage: 3.3V/5V compatible
- PCB shape: rectangular

---

## Working OLED Code Pattern

```cpp
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

TwoWire I2CBus1 = TwoWire(1);
Adafruit_SSD1306 display(128, 64, &I2CBus1, -1);

void setup() {
  I2CBus1.begin(0, 1);  // GPIO0=SDA, GPIO1=SCL
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Hello World");
  display.display();
}
```

---

## Breadboard Layout

### Large Breadboard #1
- **MCU #1** (Master Console): rows 1–8, USB-C at top
- **MCU #2** (Transaction Processor): rows 58–64, USB-C at bottom,
  board physically flipped → columns d/h are swapped vs MCU #1

### Large Breadboard #2
- **MCU #3** (Database Controller): same pattern as MCU #1
- **MCU #4** (Job Scheduler): same pattern as MCU #2

### Small Breadboard #1
- **MCU #5** (I/O Controller): alone

### Small Breadboard #2
- Shared hub: pull-up resistors, I2C bus junction, logic analyzer probes

### ESP32-C3 SuperMini footprint on breadboard
- Spans columns **d through h**
- Columns e/f/g are **inaccessible** under the board body
- Usable breakout columns: **d** and **h** only

---

## Soldering Notes

- Header pins need to be soldered onto MCUs
- Use breadboard as jig to keep pins aligned during soldering
- Use thin solder wire; heat pin and pad simultaneously
- MCUs are left **permanently seated in breadboards** to avoid
  breaking solder joints

---

## Development Environment

- **OS:** Fedora Linux
- **IDE:** VS Code + PlatformIO
- **Serial monitor:** 115200 baud
- **USB port:** `/dev/ttyACM0` (may shift to ttyACM1 after reset)
- **Logic analyzer:** 8-channel 24MHz, used with PulseView for I2C capture
- **Libraries in use:**
  - `Adafruit SSD1306` — confirmed working for OLED
  - `Adafruit GFX` — required by SSD1306
  - `Wire` / `TwoWire` — built-in, for I2C

---

## Known Hardware Quirks

- **Manual reset required** after upload — ESP32 doesn't auto-reset in
  native USB mode; press the RESET button after upload completes
- **USB port number shifts** after reset (ttyACM0 → ttyACM1);
  fixed port in platformio.ini usually works
- **Bootloader recovery:** unplug → hold BOOT → plug in USB →
  hold 2 seconds → release

---

## Current Project Status

- ✅ Dev environment working (VS Code + PlatformIO)
- ✅ All 5 MCUs verified (blink + serial monitor)
- ✅ Breadboard layout finalized
- ✅ Wiring schematic documented (wiring_schematic.html)
- ⏳ **Waiting for replacement OLED displays to arrive**
- ⬜ Task 2: OLED display test (single MCU + single display)
- ⬜ Task 3: Two MCUs communicating on shared I2C bus (GPIO8/9)
- ⬜ Task 4: All 5 MCUs on shared bus
- ⬜ Message protocol (JSON over I2C)
- ⬜ Individual subsystem firmware
- ⬜ Full system integration

---

## Development Principles

- **Baby steps with explicit pass/fail criteria** before advancing
- **One variable at a time** — compounding unknowns make debugging very hard
- **Verify with both** serial monitor and logic analyzer (PulseView)
- **Honest uncertainty** over confident-sounding guesses
- When suggesting a code fix: **stay within the task scope**, don't touch
  unrelated code
- Always **reason through and explain** changes before making them

---

## Other Hardware Available

- Wanptek WPS3010H bench power supply (30V/10A)
- Elegoo DS1307 RTC module (for Master Console, future)
- 2× SD card modules SPI (for Database Controller, future)
- Assorted LEDs, resistors (including 4.7kΩ for I2C pull-ups)
- ELM327 OBD-II adapter (future CAN bus exploration, unrelated to this project)