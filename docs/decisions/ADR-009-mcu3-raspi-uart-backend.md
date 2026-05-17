# ADR-009: Raspberry Pi 3B+ replaces SD card as MCU #3 storage backend

## Status
Accepted — 2026-05-07

## Supersedes
ADR-008 (hardware substitution — SD card approach)

---

## Context

MCU #3 (Database Controller) requires persistent storage for account balances
and an append-only transaction log. The original design used an SPI SD card
attached directly to the MCU. After three board replacements and exhaustive
debugging across multiple library configurations, card formats, and pin
combinations, the SD card approach was abandoned. See ADR-008 for the full
hardware debugging history.

The core requirement has not changed:
- Store and retrieve account balances by account number
- Append transaction log entries with UUID identifiers
- Support unbounded transaction history (rules out internal flash)
- Eventually serve transaction history via web interface

### Options considered

**Option A: Continue SD card debugging**
CMD0 now passes consistently on the ESP32-WROOM-32 DevKit. CMD8 fails with
data=0xFF on every attempt — MISO reading idle, card not driving a response.
This has been consistent across boards and wiring configurations. The cost
of continuing to chase this is not justified given the time already spent.
Rejected.

**Option B: Raspberry Pi as I2C slave replacing MCU #3 entirely**
RPi joins the shared bus at address 0x0A and runs a Python I2C slave. Solves
storage and web interface in one device. Rejected: Linux I2C slave driver
support on RPi is notoriously unreliable and would likely reproduce the same
debugging spiral in a different form.

**Option C: MCU #3 stays on bus, RPi connected over WiFi**
MCU #3 forwards DB requests to RPi over TCP/IP. Rejected: WiFi adds 20–100ms
latency per request plus connection management complexity. UART is simpler
and faster for a point-to-point link.

**Option D: MCU #3 stays on bus, RPi connected over UART**
MCU #3 remains the I2C slave at 0x0A — no change visible to any other MCU.
When it receives DB_READ or DB_WRITE, it forwards the request to RPi over
a dedicated UART link (newline-terminated JSON), waits for a response with
timeout, and returns the result on the shared bus. RPi owns SQLite and Flask.
Selected.

---

## Decision

MCU #3 retains its role as I2C slave at address 0x0A and continues to
participate in the shared bus exactly as designed. All other MCUs are
unaffected — they still send DB_READ and DB_WRITE to 0x0A and receive
DB_READ_RESULT and DB_WRITE_ACK in return.

Storage is delegated to a **Raspberry Pi 3B+** connected to MCU #3 via
**UART1** (GPIO18=TX, GPIO19=RX, shared GND). The RPi runs:
- A Python UART listener that owns all SQLite access
- A Flask web server that reads the same SQLite database
- Periodic rsync of the SQLite file to the development PC for backup

### UART protocol

Messages are newline-terminated JSON. MCU #3 sends a request, RPi responds,
MCU #3 waits up to 500ms before returning `Status::TIMEOUT` to MCU #2.

Request from MCU #3 to RPi:
```json
{"op":"read","ac":"12345678"}
{"op":"write","ac":"12345678","nb":60000,"tt":1,"mid":"uuid"}
```

Response from RPi to MCU #3:
```json
{"bal":50000,"st":0}
{"st":0}
```

### Write-ahead log pattern on RPi

For every DB_WRITE:
1. RPi writes transaction to log table with `status=PENDING`
2. RPi updates account balance
3. RPi marks transaction `status=COMMITTED`
4. RPi responds to MCU #3

On RPi boot, any `PENDING` entries that never reached `COMMITTED` indicate
an interrupted transaction. These can be replayed or flagged for review.
This mirrors the write-ahead log pattern originally planned for the SD card.

### Redundancy

SQLite WAL mode is enabled — safe concurrent access between UART listener
and Flask. Periodic rsync to development PC over WiFi provides a second
copy of the database without additional hardware.

---

## New pin assignments for MCU #3

| Function | GPIO | Notes |
|---|---|---|
| Shared bus SDA | GPIO8 | Unchanged — hub wiring unchanged |
| Shared bus SCL | GPIO9 | Unchanged — hub wiring unchanged |
| OLED SDA | GPIO16 | U8g2 SW I2C — unchanged |
| OLED SCL | GPIO17 | U8g2 SW I2C — unchanged |
| UART TX (to RPi RX) | GPIO18 | Former SD SCK pin — now UART1 TX |
| UART RX (from RPi TX) | GPIO19 | Former SD MISO pin — now UART1 RX |
| ~~SD MOSI~~ | ~~GPIO23~~ | Freed — not connected |
| ~~SD CS~~ | ~~GPIO5~~ | Freed — not connected |

RPi GPIO14 (TX) → MCU #3 GPIO19 (RX)
RPi GPIO15 (RX) → MCU #3 GPIO18 (TX)
Shared GND required.

Voltage levels match: both ESP32 and RPi GPIO are 3.3V. No level shifting needed.

---

## RPi configuration requirements

- Raspberry Pi OS Lite (no desktop)
- Hardware UART assigned to GPIO pins: add `dtoverlay=disable-bt` to
  `/boot/config.txt` — frees hardware UART (ttyAMA0) from Bluetooth,
  exposes it on GPIO14/15. Bluetooth moves to mini-UART (acceptable for
  BT bandwidth).
- Serial console on UART disabled (would conflict with application use)
- Python 3 + Flask (`pip install flask`)
- SQLite3 (included in Python standard library)
- SQLite WAL mode enabled at application startup
- Dedicated 5V/3A power supply — do NOT power from bench supply shared
  with MCUs (RPi 3B+ can draw up to 2.5A under load)

---

## Repo structure

RPi code lives in the project repo alongside MCU firmware:

```
code/
  raspi-db-server/
    CLAUDE.md         — setup steps, design notes, DoD per step
    src/
      db_server.py    — UART listener + SQLite read/write
      web_server.py   — Flask web interface
    schema/
      schema.sql      — SQLite schema (accounts + transactions tables)
```

---

## Impact on MCU #3 firmware

- SD card task replaced with UART task (same queue pattern: uartQueue +
  uartResultQueue replacing sdQueue + sdResultQueue)
- `config.h`: SD pin definitions removed, UART pin definitions added
- `platformio.ini`: SdFat removed, SDFAT_FILE_TYPE and SPI_DRIVER_SELECT
  flags removed
- Logic task: unchanged externally — still handles DB_READ/DB_WRITE,
  still puts requests on queue and waits for result
- MCU #2 logic task: `xQueueReceive` on result queue must use a timeout
  (500ms recommended) — if RPi is down, MCU #3 returns Status::TIMEOUT
  to MCU #2 rather than blocking forever

## Impact on other MCUs

None. MCU #3 still responds to DB_READ and DB_WRITE at address 0x0A.
The storage backend is an internal implementation detail.

---

## Consequences

- SD card module, SD card, and terminal adapter SD wiring are no longer
  needed for MCU #3. Hardware can be retained for future use.
- RPi requires its own dedicated power supply
- RPi must be booted before MCU #3 starts processing DB requests —
  MCU #3 handles RPi-down gracefully via UART timeout
- Web interface for transaction history is now a first-class deliverable
  (Flask on RPi), not deferred to Phase 4
- rsync redundancy to PC is simple to implement and provides backup
  without additional hardware
- Phase 5 RAID-1 dual SD card item is obsolete and removed from roadmap