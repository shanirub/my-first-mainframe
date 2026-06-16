#!/usr/bin/env python3
"""
Phase 3.2 smoke test — OLED static text via luma.oled.

What this tests:
  1. luma.core can open the I2C serial interface on /dev/i2c-1 (GPIO2/3)
  2. ssd1306 device initializes without error
  3. Static text renders and stays visible on the display

What it does NOT test:
  - Any bus traffic from MCUs
  - display_state / display_lock (that's Phase 3.8, live updates)
  - Any interaction with bus_worker

DoD for 3.2: "DATABASE CTRL / Addr: 0x0A" visible on the SSD1306, no exception raised.

UNVERIFIED — flagged explicitly:
  - luma.core.interface.serial.i2c(port=1, address=0x3C) parameter names are
    assumed, not confirmed against source. If this raises a TypeError, the
    constructor signature differs and we need to check
    luma/core/interface/serial.py directly.
  - SSD1306 I2C address assumed 0x3C (the standard default for this module).
    If wrong, the display will likely throw an I/O error on init.
"""

from luma.core.interface.serial import i2c
from luma.core.render import canvas
from luma.oled.device import ssd1306

# Assumption: port=1 maps to /dev/i2c-1 (GPIO2/3, BSC1 master) — matches
# architecture doc. address=0x3C is the standard SSD1306 default.
I2C_PORT    = 1
OLED_ADDR   = 0x3C

def main():
    print("Opening I2C serial interface on /dev/i2c-1...")
    serial = i2c(port=I2C_PORT, address=OLED_ADDR)
    print("OK: serial interface opened")

    print("Initializing SSD1306 device...")
    device = ssd1306(serial, width=128, height=64, rotate=0)
    print("OK: SSD1306 initialized")

    print("Drawing static text...")
    with canvas(device) as draw:
        draw.text((0, 0),  "DATABASE CTRL",  fill="white")
        draw.text((0, 16), "Addr: 0x0A",     fill="white")
        draw.text((0, 32), "Phase 3.2",      fill="white")
        draw.text((0, 48), "Smoke Test OK",  fill="white")
    print("OK: text drawn — check the physical display now")

    print("\n--- Phase 3.2 PASS (pending visual confirmation) ---")
    print("Holding 15s — verify text is visible on the OLED.")

    import time
    time.sleep(15)

if __name__ == "__main__":
    main()