# ADR-008: WeMos LOLIN32 Lite replaces ESP32-C3 SuperMini for MCU #3

## Status
Accepted — 2026-04-16, amended 2026-04-18

## Reference Documents

**ESP32-C3 (replaced board):**
- ESP32-C3 Series Datasheet: https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf
- ESP32-C3 Technical Reference Manual: https://www.espressif.com/sites/default/files/documentation/esp32-c3_technical_reference_manual_en.pdf
- ESP32-C3-MINI-1 Module Datasheet: https://documentation.espressif.com/esp32-c3-mini-1_datasheet_en.pdf

**WeMos LOLIN32 Lite (replacement board):**
- ESP32 Series Datasheet (covers ESP32-D0WDQ6): https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf
- ESP32 Technical Reference Manual: https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf
- WeMos LOLIN32 Lite pinout and specs (Mischianti): https://mischianti.org/esp32-wemos-lolin32-lite-high-resolution-pinout-and-specs/

---

## Context

MCU #3 (Database Controller) requires four peripherals to run simultaneously:

| Peripheral | Pins needed |
|---|---|
| Shared inter-MCU I2C bus | GPIO8=SDA, GPIO9=SCL |
| Private OLED display (U8g2 SW_I2C) | 2 GPIOs |
| SPI SD card (MOSI, MISO, SCK, CS) | 4 GPIOs |

The ESP32-C3 SuperMini exposes 11 GPIO pins total. Per the ESP32-C3 Series
Datasheet and Technical Reference Manual, the following pins are unavailable
for general user SPI:

| GPIO | Reason unavailable |
|---|---|
| GPIO6 | FSPICLK — flash SPI clock, used in all flash modes |
| GPIO7 | FSPID — flash SPI data, used in Dual I/O and Quad I/O flash modes |
| GPIO11 | FSPICS0 — flash SPI chip select |
| GPIO12–17 | SPI0/SPI1 — internal flash interface, not broken out on SuperMini |
| GPIO4–7 | Shared with JTAG interface (per TRM Chapter IO MUX) |
| GPIO0 | Strapping pin — boot mode selection |
| GPIO2 | Strapping pin — avoid pulling low at boot |
| GPIO8 | Strapping pin + onboard LED (already in use: shared bus SDA) |
| GPIO9 | BOOT button (already in use: shared bus SCL) |

After subtracting the shared bus (GPIO8/9) and OLED (GPIO3/10), the
nominally remaining pins are GPIO0, GPIO1, GPIO2, GPIO5, GPIO20, GPIO21.
However, GPIO0 and GPIO2 are strapping pins with boot-mode sensitivity,
leaving GPIO1, GPIO5, GPIO20, GPIO21 as the only clean candidates —
exactly 4 pins for 4 SPI signals, with zero margin.

In practice, extensive testing confirmed the chip does not behave reliably
with user SPI on these pins in combination:

- **GPIO6/7 combination (first attempt):** confirmed dead — internal flash
  pins show 0V externally regardless of firmware configuration
- **GPIO5/20 combination:** SdFat error 0x17 (CMD0 — card never responded),
  crashes in `spiTransferByteNL` and `cpu_ll_waiti`
- **GPIO0/1 combination:** same CMD0 failure; additionally GPIO0 affected
  boot mode (boot:0xe vs expected boot:0xf)

A SPI loopback test (MOSI jumpered to MISO) passed on all combinations,
confirming the SPI2 peripheral hardware itself is functional. The failure
is specific to the SD card communication path — most likely due to
interactions between the GPIO matrix, JTAG peripheral, and flash controller
that only manifest under active SPI bus traffic.

The ESP32-C3 Technical Reference Manual notes that SPI2 pins are multiplexed
with GPIO2, GPIO4–GPIO7, GPIO10, and the JTAG interface via the IO MUX
(TRM Section "SPI Controller"). Even pins not used for flash at runtime may
have residual contention from the IO MUX configuration established at boot.

**Conclusion:** The ESP32-C3 SuperMini does not have sufficient reliably-usable
GPIOs to run shared I2C bus + OLED + SPI SD card simultaneously in practice,
even though a naive pin count suggests it should be possible. This is a
silicon-level constraint exposed through testing, not a firmware issue.

---

## Decision

Replace the ESP32-C3 SuperMini with a **WeMos LOLIN32 Lite** (or compatible
clone — the original is discontinued) for MCU #3 only. The board in use is
a clone based on the ESP32-D0WDQ6 chip with CH340C USB-to-serial, PCB
antenna, single RST button, and JST LiPo connector. All other MCUs remain
as ESP32-C3 SuperMini.

MCU #3 retains its I2C address (0x0A), subsystem role (Database Controller),
and firmware architecture (FreeRTOS tasks, SharedBus, MessageProtocol).
Only the hardware and pin assignments change.

The LOLIN32 Lite breaks out the ESP32's VSPI bus with explicit labels on the
PCB (V_SPI_CLK, V_SPI_Q/MISO, V_SPI_D/MOSI, V_SPI_CS0/SS) — clean
general-purpose pins confirmed against the board pinout with no flash or
JTAG conflicts. This gives ample headroom for all four peripherals.

**Note:** GPIO5 has an onboard blue LED on the LOLIN32 Lite (labeled
V_SPI_CS0/SS on the pinout). Using GPIO5 as SD CS means the LED will
flicker during SPI activity. This is cosmetic — it does not affect function.

---

## New pin assignments for MCU #3

| Function | GPIO | LOLIN32 Lite label | Notes |
|---|---|---|---|
| Shared bus SDA | GPIO8 | GPIO8 | Matches all other MCUs — hub wiring unchanged |
| Shared bus SCL | GPIO9 | GPIO9 | Matches all other MCUs — hub wiring unchanged |
| OLED SDA | GPIO16 | RXD2 | U8g2 SW I2C — free, no special functions |
| OLED SCL | GPIO17 | TXD2 | U8g2 SW I2C — free, no special functions |
| SD MOSI | GPIO23 | V_SPI_D / MOSI | ESP32 default VSPI MOSI |
| SD MISO | GPIO19 | V_SPI_Q / MISO | ESP32 default VSPI MISO |
| SD SCK  | GPIO18 | V_SPI_CLK / SCK | ESP32 default VSPI SCK |
| SD CS   | GPIO5  | V_SPI_CS0 / SS  | ESP32 default VSPI CS — onboard LED on this pin |

---

## Impact on shared_config.h

`OLED_SDA_PIN` and `OLED_SCL_PIN` were previously defined in `shared_config.h`
as GPIO3 and GPIO10 — valid for all ESP32-C3 SuperMinis. With MCU #3 now on
a different board, these values are no longer universal. They have been moved
to each MCU's local `config.h`.

MCUs #1, #2, #4, #5 keep GPIO3=SDA, GPIO10=SCL as before.
MCU #3 uses GPIO16=SDA, GPIO17=SCL.

GPIO3 on the LOLIN32 Lite is RX0 (UART0 receive). GPIO10 is connected to
the internal flash SPI interface. Both are unsafe for OLED use on this board.

`shared_config.h` continues to own all values that are truly shared: shared
bus pins (GPIO8/9), MCU addresses, FreeRTOS stack sizes, and heartbeat timing.

---

## SD card library

The Arduino `SD.h` library is used for MCU #3. SdFat exhibited unstable
behaviour during the ESP32-C3 debugging process — crashes in
`spiTransferByteNL` across multiple pin combinations — though this may have
been a consequence of the pin conflicts rather than a library incompatibility.
SD.h with FAT32 on the LOLIN32 Lite is the proven path forward.

SD card is formatted as FAT32 with an 8GB partition on a 64GB physical card.
FAT32 volumes over 32GB are not reliably supported by SD.h or SdFat. The
remaining ~56GB is unallocated. For this project, MCU #3 needs only a few MB.

---

## Consequences

- MCU #3 breadboard replaced with a larger breadboard to accommodate the
  LOLIN32 Lite form factor and leave space for a second SD card module
  (planned for Phase 5 RAID-1 storage redundancy — hardware already available)
- The original ESP32-C3 SuperMini that was MCU #3 is retained for possible
  future use
- platformio.ini for MCU #3: board = esp32dev, upload/monitor port =
  /dev/ttyUSB0 (CH340C USB-to-serial bridge on the board)
- OLED pin constants moved from shared_config.h to per-MCU config.h
- All other MCU firmware, shared libraries, and physical wiring unchanged

## Amendment — 2026-04-18: WeMos LOLIN32 Lite also replaced

The board received and soldered as "ESP32 DevKit" was in fact a **WeMos
LOLIN32 Lite clone** (ESP32-D0WDQ6, CH340C, PCB antenna). The AliExpress
listing was misleading.

The LOLIN32 Lite was found to be incompatible with this project for a
different but equally fundamental reason: **GPIO8 and GPIO9 are connected
to the 32kHz crystal oscillator (Xtal32N/Xtal32P) on this board and are
not available as user GPIO.** The shared inter-MCU I2C bus requires
GPIO8=SDA and GPIO9=SCL on all MCUs — this is a hard architectural
constraint documented in shared_config.h and the hub wiring.

SD card debugging was also attempted on this board. Consistent SdFat
errors (0x17, 0x0C, 0x01) were observed. Breadboard contact failures
caused intermittent behavior masking the real errors. After bypassing
the breadboard with direct jumpers, errors became consistent (0x01 /
CMD1 timeout) but the GPIO8/9 issue made the board a dead end regardless.

**Second replacement: ESP32-WROOM-32 DevKit (38-pin, CP2102, Type-C USB)**
This board has GPIO8/9 confirmed available as user GPIO per the official
Espressif pinout. All other pin assignments (OLED GPIO16/17, SD card
GPIO18/19/23/5, shared bus GPIO8/9) remain identical. PlatformIO target
remains `board = esp32dev`. No firmware changes required.

An ESP32 Terminal Adapter (screw terminals) has also been ordered to
eliminate breadboard contact failures permanently for MCU #3.