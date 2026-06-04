# raspi-db-server — Raspberry Pi Database Controller

> This file replaces the previous CLAUDE.md which described a UART-based backend
> (ADR-009). That architecture is void. The RPi now participates directly on the
> shared I2C bus as a pure slave. See ADR-009 and ADR-011 for the full history.

---

## Identity

| Field | Value |
|-------|-------|
| Board | Raspberry Pi 3B+ |
| Role | Database Controller |
| I2C address | 0x0A |
| Bus participation | Pure I2C slave — never initiates |
| Connection | SSH — raspi-db-server (Tailscale) |
| OS | Raspberry Pi OS Lite (64-bit) |

---

## Hardware

| Peripheral | GPIO | BCM2837 peripheral | Role |
|-----------|------|--------------------|------|
| Shared inter-MCU I2C bus | GPIO18 (SDA), GPIO19 (SCL) | BSC slave (`pigpio bsc_i2c`) | Receive DB_READ, DB_WRITE, HEARTBEAT |
| Private OLED display | GPIO2 (SDA), GPIO3 (SCL) | BSC1 master (`luma.oled`, `/dev/i2c-1`) | Drive SSD1306 128×64 |

GPIO18/19 and GPIO2/3 are **separate hardware peripherals** on the BCM2837 SoC. They do
not interfere with each other. Only GPIO18/19 connects to the shared bus hub. GPIO2/3 is
a private wire to the OLED.

**Power:** Dedicated 5V/3A USB supply. Do NOT power from the bench supply shared with
the MCUs.

---

## Wiring

**Shared bus (GPIO18/19 → hub):**
```
RPi GPIO18 (SDA) → shared bus hub SDA rail
RPi GPIO19 (SCL) → shared bus hub SCL rail
RPi GND          → shared bus hub GND rail
```
Pull-ups: existing 5kΩ resistors on hub SDA and SCL — unchanged. RPi GPIO18/19 are 3.3V,
matching the ESP32-C3. No level shifting needed.

**OLED (GPIO2/3 → SSD1306, private):**
```
RPi GPIO2 (SDA) → OLED SDA
RPi GPIO3 (SCL) → OLED SCL
RPi 3.3V        → OLED VCC
RPi GND         → OLED GND
```
This wire pair does NOT connect to the shared bus hub.

**UART wiring (GPIO14/15) — remove:**
The UART wiring from ADR-009 (RPi GPIO14/15 ↔ former MCU #3 GPIO18/19) is no longer
needed. Remove after confirming the I2C wiring above is working.

---

## RPi OS Setup

### Step 1 — OS install
Flash Raspberry Pi OS Lite (64-bit) using Raspberry Pi Imager. Enable SSH in Imager
advanced options. Set hostname (`raspi-db-server`), username, password, WiFi.
DoD: `ssh pi@raspi-db-server` connects successfully (via Tailscale once enrolled).

### Step 2 — Tailscale
```bash
curl -fsSL https://tailscale.com/install.sh | sh
sudo tailscale up
```
DoD: device appears in Tailscale admin console as `raspi-db-server`. SSH works from
dev PC without local network.

### Step 3 — Enable pigpio daemon
```bash
sudo apt update && sudo apt install -y pigpio
sudo systemctl enable pigpiod
sudo systemctl start pigpiod
```
Verify:
```bash
systemctl is-active pigpiod   # should print: active
```
DoD: `pigpiod` starts on boot and is active before any Python service runs.

### Step 4 — Install Python dependencies (Phase 3)
```bash
pip3 install pigpio luma.oled
```
DoD: `python3 -c "import pigpio; from luma.oled.device import ssd1306; print('ok')"` prints ok.

### Step 5 — Install Python dependencies (Phase 4, deferred)
```bash
pip3 install flask
```
`sqlite3` is included in the Python standard library — no install needed.

### Step 6 — UART setup (skip — no longer needed)
The `disable-bt` overlay and serial console steps from ADR-009 are not required.
GPIO14/15 (ttyAMA0) was used for UART to MCU #3, which is retired. Leave UART
unconfigured unless needed for another purpose.

---

## File Structure

```
code/raspi-db-server/
  CLAUDE.md           — this file: setup, design notes, DoD per phase
  src/
    main.py           — startup sequence, thread creation, signal.pause()
    bus_slave.py      — pigpio bsc_i2c setup, EVENT_BSC callback, TX FIFO protocol
    oled.py           — luma.oled SSD1306 driver, oled_worker thread
    protocol.py       — message type constants, JSON builders (mirrors MessageProtocol)
    db.py             — SQLite read/write, WAL mode (Phase 4+)
    web_server.py     — Flask transaction history at :5000 (Phase 4+)
  schema/
    schema.sql        — accounts + transactions tables (Phase 4+)
```

---

## Thread Architecture

Three concurrent Python threads, mirroring the FreeRTOS task + queue pattern on the MCUs.
Full design in `docs/design/raspi_architecture.md`.

| Thread | FreeRTOS equivalent | Phase | Blocks on |
|--------|--------------------|----|-----------|
| `bus_worker` | Logic task (pri=2) | 3 | `bus_queue.get()` — wakes on incoming bus message |
| `oled_worker` | OLED task (pri=1) | 3 | `time.sleep(0.5)` — 500ms refresh |
| `flask` | HTTP server task | 4 | `app.run()` — deferred |

**Event path:**
```
pigpio EVENT_BSC callback (C thread, outside Python process)
  → reads RX FIFO via bsc_i2c()
  → bus_queue.put(raw_bytes)          ← queue.Queue, thread-safe handoff
bus_worker thread
  → bus_queue.get()                   ← blocks until work arrives
  → parse JSON, handle request
  → load_tx_fifo(response)
  → update display_state under display_lock
oled_worker thread
  → copy display_state under display_lock
  → oled.show(snapshot)               ← I2C write to OLED, outside lock
```

**Shared state (protected by `display_lock`):**
```python
display_state = {
    "status":       "IDLE",   # "IDLE" | "DB_READ" | "DB_WRITE" | "HEARTBEAT"
    "last_account": "",       # 8-digit string, empty until first request
    "last_result":  "",       # "OK" | "NOT_FOUND" | "TIMEOUT" | "ERROR"
}
```
Lock is held for the minimum time to read or write `display_state` — never during
I/O (SQLite, OLED, I2C).

---

## TX FIFO Protocol

The MCU always reads exactly 256 bytes from the RPi. Byte 0 is the ready flag; bytes
1–255 carry the JSON response payload (null-padded).

```python
READY     = b'\x01'
NOT_READY = b'\x00'

def load_tx_fifo(response_json: str):
    payload = response_json.encode('utf-8')
    buf = READY + payload[:255]
    buf = buf.ljust(256, b'\x00')
    pi.bsc_i2c(I2C_ADDR, buf)       # loads TX FIFO, ready_byte = 0x01

def clear_tx_fifo():
    pi.bsc_i2c(I2C_ADDR, NOT_READY + b'\x00' * 255)
```

`clear_tx_fifo()` is called at startup and after each response is delivered, ensuring
the ready flag is `0x00` between transactions. This prevents an MCU from reading a
stale response from a previous transaction.

The MCU polls `requestFrom(0x0A, 256)` in a loop (10ms intervals, 500ms total timeout)
until byte[0] == `0x01`. For HEARTBEAT, a fixed 20ms delay is used instead of a loop.

---

## protocol.py — Sync Warning

`protocol.py` declares message type constants and status codes that mirror the C++
`MessageProtocol` shared library. There is no shared source of truth — these are
maintained in parallel manually.

⚠️ **If you change any constant value in `MessageProtocol` (C++ side), you must update
`protocol.py` on the RPi side to match.** Silent divergence will cause the RPi to
misparse message types or return wrong status codes with no obvious error.

Check both files any time a protocol constant is added or modified.

---

## Phase 3 — Definition of Done

| Step | Task | DoD |
|------|------|-----|
| 3.1 | pigpio BSC slave init | `bsc_i2c(0x0A)` returns without error; pigpio daemon running; no crash on startup |
| 3.2 | OLED smoke test | Static "DATABASE CTRL / Addr: 0x0A" visible on SSD1306 |
| 3.3 | HEARTBEAT handling | MCU #1 sends HEARTBEAT to 0x0A; RPi returns HEARTBEAT_ACK; MCU #1 OLED shows RPi as active |
| 3.4 | sendAndReceive() on MCU #1 | MCU #1 uses sendAndReceive() for HEARTBEAT to RPi; confirmed on logic analyzer |
| 3.5 | DB_READ stub | MCU #2 sends DB_READ; RPi returns hardcoded DB_READ_RESULT with balance=50000, status=OK |
| 3.6 | DB_WRITE stub | MCU #2 sends DB_WRITE; RPi returns hardcoded DB_WRITE_ACK, status=OK |
| 3.7 | sendAndReceive() on MCU #2 | MCU #2 uses sendAndReceive() for DB_READ and DB_WRITE; confirmed on logic analyzer |
| 3.8 | OLED live updates | OLED reflects current status and last account number on each bus event |

All Phase 3 stubs are in `bus_slave.py` / `main.py`. The thread structure, TX FIFO
protocol, and ready-flag convention are all live — only the DB backend is stubbed.

---

## Phase 4 — Items (detail in roadmap.md)

- `db.py`: SQLite read/write, WAL mode, write-ahead log pattern (PENDING → COMMITTED)
- `web_server.py`: Flask at port 5000, transaction history table, read-only
- Account seeding: populate accounts table before first use
- `rsync` cron job: backup `mainframe.db` to dev PC every 10 minutes

Phase 4 replaces the stub calls in `bus_worker` with `db.read()` and `db.write()`.
Thread structure and protocol do not change.

---

## Phase 5 — Items (detail in roadmap.md)

- Crash recovery: on boot, detect any `PENDING` rows and replay them
- Two-phase commit support for TRANSFER (if implemented)

---

## Critical Notes

- **pigpiod must be running before the Python service starts.** If pigpiod is not active,
  `bsc_i2c()` calls will fail. Use systemd to ensure ordering.

- **BSC slave is GPIO18/19 only.** This is a hardware constraint of the BCM2837 — the
  BSC slave peripheral is hardwired to these pins in silicon. Cannot be changed in
  software. Do not confuse with BSC1 master on GPIO2/3 (OLED only).

- **RPi must be booted before MCU #1 and MCU #2 start processing.** Both handle RPi-down
  gracefully via the 500ms sendAndReceive() timeout, but HEARTBEAT misses will be logged
  until the RPi is online.

- **protocol.py constants must match MessageProtocol C++ values.** See sync warning above.

- **All monetary values are integer cents.** Never use float for balances. $100.00 = 10000.

- **SQLite WAL mode must be enabled at startup** (Phase 4+):
  `PRAGMA journal_mode=WAL` — allows Flask (reader) and bus_worker (writer) to access
  the DB concurrently without blocking each other.

- **One SQLite connection per thread** (Phase 4+). `bus_worker` holds its own connection;
  Flask holds its own. Do not share connections across threads.