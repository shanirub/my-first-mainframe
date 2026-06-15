#!/usr/bin/env python3
"""
Phase 3.1 smoke test — BSC slave init on GPIO18/19.

What this tests:
  1. pigpio daemon is reachable (pigpio.pi() connects)
  2. bsc_i2c(0x0A) arms the BSC slave without error
  3. clear_tx_fifo loads NOT_READY + zeros into TX FIFO without error

What it does NOT test:
  - Any bus traffic from MCUs
  - EVENT_BSC callback
  - Any other thread

DoD for 3.1: all three steps print OK, no exception raised.
"""

import pigpio
import time

I2C_ADDR  = 0x0A
NOT_READY = b'\x00'

def main():
    # Step 1 — connect to pigpio daemon
    # Assumption: pigpiod is running locally, no-args constructor connects to 127.0.0.1:8888
    print("Connecting to pigpio daemon...")
    pi = pigpio.pi()

    # pi.connected is the standard check — not in index, using attribute access directly.
    # If the daemon is not running, pigpio.pi() does not raise — it returns a pi object
    # with pi.connected == False. We must check explicitly.
    if not pi.connected:
        print("FAIL: pigpio daemon not reachable. Is pigpiod running?")
        return
    print("OK: connected to pigpio daemon")

    try:
        # Step 2 — arm BSC slave at 0x0A
        # bsc_i2c(i2c_address, data): arming with empty data (b'') sets up the slave
        # without loading anything into the TX FIFO yet.
        # Return value is a status int — negative means error (pigpio convention).
        print("Arming BSC slave at 0x0A...")
        status = pi.bsc_i2c(I2C_ADDR, b'')
        print(f"  bsc_i2c() returned: {status}")
        if isinstance(status, int) and status < 0:
            print(f"FAIL: bsc_i2c returned error code {status}")
            return
        print("OK: BSC slave armed")

        # Step 3 — load NOT_READY into TX FIFO
        # This is clear_tx_fifo() from the architecture doc.
        # Ensures byte[0] == 0x00 so no MCU reads a stale response.
        print("Loading NOT_READY into TX FIFO...")
        buf = NOT_READY + b'\x00' * 255   # exactly 256 bytes
        status = pi.bsc_i2c(I2C_ADDR, buf)
        print(f"  bsc_i2c() returned: {status}")
        if isinstance(status, int) and status < 0:
            print(f"FAIL: clear_tx_fifo returned error code {status}")
            return
        print("OK: TX FIFO cleared (NOT_READY loaded)")

        print("\n--- Phase 3.1 PASS ---")
        print("BSC slave is armed at 0x0A. Ready for Phase 3.2.")

        # Hold for 5 seconds so you can observe with logic analyzer if attached
        print("Holding 5s (attach logic analyzer if needed)...")
        time.sleep(5)

    finally:
        # Disarm BSC slave before exit — bsc_i2c(0, b'') with address=0 disarms
        # (pigpio convention: address 0 = release the peripheral).
        # Uncertain: disarm convention not verified in index. Flagging explicitly.
        # If this causes an error, comment it out — it's cleanup only.
        print("Disarming BSC slave...")
        pi.bsc_i2c(0, b'')
        pi.stop()
        print("OK: pigpio connection closed")

if __name__ == "__main__":
    main()