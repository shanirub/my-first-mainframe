# raspi-db-server — Raspberry Pi DB Backend

## Role
Storage and web interface backend for MCU #3 (Database Controller).
Receives DB requests from MCU #3 over UART, reads/writes SQLite,
serves transaction history over Flask.

This is not a standalone service — it is the storage backend for the
shared bus DB controller. MCU #3 at address 0x0A is the bus-facing
participant; this RPi script is the implementation behind it.

## Hardware
- Board: Raspberry Pi 3B+
- Connected to: MCU #3 ESP32-WROOM-32 DevKit via UART
- Storage: RPi's own SD card (boot drive) — no external SD module
- Power: dedicated 5V/3A USB supply — do NOT share with bench supply

## Wiring
| RPi pin | GPIO | Direction | MCU #3 GPIO |
|---------|------|-----------|-------------|
| GPIO14 | TX | RPi→MCU | GPIO19 (RX) |
| GPIO15 | RX | MCU→RPi | GPIO18 (TX) |
| GND | — | shared | GND |

Voltage: both sides are 3.3V — no level shifting needed.

## Repo Structure
```
code/raspi-db-server/
  CLAUDE.md         — this file
  src/
    db_server.py    — UART listener + SQLite read/write
    web_server.py   — Flask web interface
  schema/
    schema.sql      — SQLite schema
```

## UART Protocol
Newline-terminated JSON. RPi reads until `\n`, processes, writes response + `\n`.
Baud rate: 115200. Hardware UART (ttyAMA0) on GPIO14/15.

Requests from MCU #3:
```json
{"op":"read","ac":"12345678"}
{"op":"write","ac":"12345678","nb":60000,"tt":1,"mid":"uuid"}
```

Responses to MCU #3:
```json
{"bal":50000,"st":0}
{"st":0}
```

Status codes match MessageProtocol `Status::` constants:
- 0 = OK
- 3 = NOT_FOUND (account does not exist)
- 5 = DB_ERROR (SQLite failure)

## SQLite Schema
```sql
CREATE TABLE accounts (
    account_id TEXT PRIMARY KEY,  -- 8-digit string e.g. "12345678"
    balance    INTEGER NOT NULL    -- cents, never float
);

CREATE TABLE transactions (
    id         TEXT PRIMARY KEY,   -- UUID v4 from MCU message mid field
    account_id TEXT NOT NULL,
    txn_type   INTEGER NOT NULL,   -- matches TxnType constants: 1=DEPOSIT 2=WITHDRAW 4=BALANCE
    amount     INTEGER NOT NULL,   -- cents
    new_bal    INTEGER NOT NULL,   -- balance after transaction
    status     TEXT NOT NULL,      -- PENDING or COMMITTED
    ts         DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

Write-ahead log pattern: every DB_WRITE inserts a PENDING transaction row,
updates the balance, then marks the row COMMITTED — all in a single SQLite
transaction (atomic). On RPi boot, any PENDING rows indicate an interrupted
write and can be flagged or replayed.

## RPi Setup Steps

### Step 1 — OS install
Flash Raspberry Pi OS Lite (64-bit) to RPi SD card using Raspberry Pi Imager.
Enable SSH in Imager advanced options. Set hostname, username, password, WiFi.
DoD: `ssh pi@raspberrypi.local` connects successfully.

### Step 2 — Free hardware UART from Bluetooth
Add to `/boot/firmware/config.txt` (or `/boot/config.txt` on older images):
```
dtoverlay=disable-bt
```
Disable serial console (it would conflict with application use):
```bash
sudo raspi-config
# Interface Options → Serial Port
# "Would you like a login shell to be accessible over the serial port?" → No
# "Would you like the serial port hardware to be enabled?" → Yes
```
Reboot. Confirm hardware UART is on GPIO14/15:
```bash
ls -la /dev/ttyAMA0
```
DoD: `/dev/ttyAMA0` exists and is not in use by console.

### Step 3 — Install dependencies
```bash
sudo apt update && sudo apt install -y python3-pip
pip3 install flask
```
SQLite3 is included in Python standard library — no install needed.
DoD: `python3 -c "import flask, sqlite3; print('ok')"` prints ok.

### Step 4 — UART echo test
Run a minimal Python echo script on RPi, wire to MCU #3, run MCU #3 Step 2
(UART echo test). Confirm round-trip.
DoD: MCU #3 serial shows echo received from RPi.

### Step 5 — DB server: read handler
Implement `db_server.py` with `op=read` handler. Create SQLite database,
insert a test account. Run MCU #3 Step 4 (DB_READ flow).
DoD: MCU #3 receives correct balance from RPi over UART.

### Step 6 — DB server: write handler
Add `op=write` handler with write-ahead log pattern. Run MCU #3 Step 5
(DB_WRITE flow).
DoD: balance updated in SQLite, transaction row present with COMMITTED status.

### Step 7 — Flask web interface
Implement `web_server.py`. Serve a table of recent transactions from the
transactions table. Run as a second process or thread alongside db_server.py.
DoD: browser on dev PC shows transaction history at http://raspberrypi.local:5000

### Step 8 — rsync backup to PC
Add a cron job on RPi:
```bash
crontab -e
# Add:
*/10 * * * * rsync -az /home/pi/mainframe.db user@devpc:/path/to/backup/
```
DoD: SQLite file appears on dev PC and updates every 10 minutes.

## Critical Notes
- Use hardware UART (ttyAMA0) not mini-UART (ttyS0) — mini-UART baud rate
  is tied to CPU clock speed and is unstable at 115200
- SQLite WAL mode must be enabled at startup: `PRAGMA journal_mode=WAL`
  — allows Flask (reader) and db_server (writer) to access the DB concurrently
- All monetary values are integer cents — never use float for balances
- SdFat file path convention does not apply here — standard Python file I/O
- RPi must be running before MCU #3 starts serving requests — MCU #3 handles
  RPi-down via 500ms UART timeout returning Status::TIMEOUT to MCU #2
