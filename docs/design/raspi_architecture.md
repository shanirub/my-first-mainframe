# Raspberry Pi Architecture — Database Controller (0x0A)

## Overview

The Raspberry Pi 3B+ replaces MCU #3 as the Database Controller on the
shared inter-MCU I2C bus. It runs a Python service structured as three
concurrent threads, mirroring the FreeRTOS task + queue pattern used on
the MCUs. See ADR-011 for the full reasoning behind this architecture.

Unlike the MCUs, the RPi runs a standard Linux OS (Raspberry Pi OS Lite).
Concurrency is provided by Python's `threading` module (preemptive, OS-
scheduled) rather than FreeRTOS (cooperative, RTOS-scheduled). The mental
model is intentionally the same: threads are tasks, `queue.Queue` is the
inter-thread queue, `threading.Lock` is the mutex.

---

## Hardware

| Bus | Peripheral | GPIO | Role |
|-----|-----------|------|------|
| Shared inter-MCU I2C | BCM2837 BSC slave | GPIO18 (SDA), GPIO19 (SCL) | Receive DB_READ, DB_WRITE, HEARTBEAT from bus |
| Private OLED | BCM2837 BSC1 master | GPIO2 (SDA), GPIO3 (SCL) | Drive SSD1306 display |

GPIO18/19 and GPIO2/3 are separate hardware peripherals on the BCM2837.
They do not interfere with each other and connect to separate physical wires.
Only GPIO18/19 connects to the shared bus hub.

---

## Thread Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     pigpio daemon (C thread)                    │
│              runs outside the Python process                    │
└──────────────────────────┬──────────────────────────────────────┘
                           │ EVENT_BSC fires on bus activity
                           │ on_bsc_event() callback:
                           │   read RX FIFO via bsc_i2c()
                           │   bus_queue.put(raw_bytes)   ← non-blocking
                           ▼
                  ┌─────────────────┐
                  │   bus_queue     │  queue.Queue(maxsize=8)
                  │                 │  each slot: raw bytes from BSC RX FIFO
                  └────────┬────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  bus_worker thread                                               │
│                                                                  │
│  while True:                                                     │
│    raw = bus_queue.get()          # blocks — no CPU consumed     │
│    request = parse_json(raw)                                     │
│    response = handle(request)     # DB query (Phase 4+)         │
│                                   # or stub response (Phase 3)  │
│    load_tx_fifo(response)         # bsc_i2c(ADDR, payload)      │
│    with display_lock:                                            │
│      display_state.update(request, response)                    │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│  oled_worker thread                                              │
│                                                                  │
│  while True:                                                     │
│    with display_lock:                                            │
│      snap = copy(display_state)                                  │
│    oled.show(snap)                # luma.oled SSD1306 driver    │
│    time.sleep(0.5)                                               │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│  flask thread  (Phase 4+)                                        │
│                                                                  │
│  app.run(host='0.0.0.0', port=5000)                              │
│  reads SQLite directly — WAL mode allows concurrent reads        │
└──────────────────────────────────────────────────────────────────┘
```

---

## Thread Summary

| Thread | Priority | Equivalent FreeRTOS task | Phase |
|--------|----------|--------------------------|-------|
| bus_worker | High (OS default) | Logic task (pri=2) | 3 |
| oled_worker | Low (OS default) | OLED task (pri=1) | 3 |
| flask | Low (OS default) | HTTP server task | 4 |

Python threads are preemptively scheduled by the OS. Unlike FreeRTOS,
there is no explicit numeric priority assignment — the OS scheduler
manages time-slicing. The `bus_worker` thread naturally gets priority
because it blocks on `queue.Queue.get()` and only runs when work arrives,
while `oled_worker` sleeps 500ms between iterations.

---

## Shared State

```python
# Protected by display_lock — same pattern as FreeRTOS displayMutex.
# Written by bus_worker, read by oled_worker and Flask.

display_lock = threading.Lock()

display_state = {
    "status":       "IDLE",      # "IDLE" | "DB_READ" | "DB_WRITE" | "HEARTBEAT"
    "last_account": "",          # 8-digit account string, empty if none yet
    "last_result":  "",          # "OK" | "NOT_FOUND" | "TIMEOUT" | "ERROR"
}
```

`display_lock` is held for the minimum time needed to read or write
`display_state` — never while doing I/O (SQLite, OLED, I2C).

---

## TX FIFO Protocol

The MCU always reads exactly 256 bytes from the RPi. Byte 0 is the ready
flag; bytes 1–255 carry the JSON response payload (padded with null bytes).

```python
READY     = b'\x01'
NOT_READY = b'\x00'

def load_tx_fifo(response_json: str):
    payload = response_json.encode('utf-8')
    # Truncate if over 255 bytes — should not happen given message sizes
    buf = READY + payload[:255]
    buf = buf.ljust(256, b'\x00')   # pad to exactly 256 bytes
    pi.bsc_i2c(I2C_ADDR, buf)       # loads TX FIFO, sets ready_byte = 0x01

def clear_tx_fifo():
    pi.bsc_i2c(I2C_ADDR, NOT_READY + b'\x00' * 255)
```

`clear_tx_fifo()` is called at startup and after each response is delivered,
ensuring the ready flag is 0x00 between transactions. This prevents the MCU
from reading a stale response from a previous transaction.

---

## Message Handling

The `bus_worker` handles all message types. Logic is sequential — one
request processed at a time, matching the MCU #2 Phase 3 sequential model.

```python
def handle(request: dict) -> str:
    op = request.get("tp")   # message type — matches MsgType constants

    if op == MsgType.HEARTBEAT:
        return build_heartbeat_ack()

    elif op == MsgType.DB_READ:
        account = request["p"]["ac"]
        # Phase 3: return stub response
        # Phase 4+: return db.read(account)
        return build_db_read_result(account, balance=50000, status=Status.OK)

    elif op == MsgType.DB_WRITE:
        account  = request["p"]["ac"]
        new_bal  = request["p"]["nb"]
        txn_type = request["p"]["tt"]
        msg_id   = request["mid"]
        # Phase 3: return stub ACK
        # Phase 4+: return db.write(account, new_bal, txn_type, msg_id)
        return build_db_write_ack(account, status=Status.OK)

    else:
        return build_error(f"unknown type: {op}")
```

Phase 3 stubs return hardcoded values. Phase 4 replaces the stub calls with
`db.read()` and `db.write()` from `db.py` — the thread structure and
protocol do not change.

---

## SQLite Strategy (Phase 4+)

SQLite is accessed exclusively from `bus_worker`. Flask reads the database
directly in its own thread.

Concurrency model:
- `bus_worker` is the sole writer. No write contention possible.
- Flask is a reader only. WAL mode (Write-Ahead Log) allows concurrent
  reads while `bus_worker` is writing — no blocking between threads.
- One SQLite connection per thread (`check_same_thread=True`, default).
  `bus_worker` holds its own connection; Flask holds its own.

```python
# bus_worker connection (opened once at thread start)
conn = sqlite3.connect("mainframe.db", check_same_thread=True)
conn.execute("PRAGMA journal_mode=WAL")
conn.execute("PRAGMA foreign_keys=ON")
```

Write-ahead log pattern for DB_WRITE (atomic — single SQLite transaction):
1. Insert transaction row with `status='PENDING'`
2. Update account balance
3. Update transaction row to `status='COMMITTED'`

On RPi boot, any `PENDING` rows that never reached `COMMITTED` indicate an
interrupted write. Phase 5 adds replay logic for these rows.

---

## OLED Display

Driver: `luma.oled` with SSD1306 on I2C bus 1 (GPIO2/3, `/dev/i2c-1`).

```
DATABASE CTRL
Addr: 0x0A
DB READ / IDLE
Acct: 12345678
```

Line 3 reflects `display_state["status"]` — updates on every bus event.
Line 4 shows `display_state["last_account"]` — blank until first request.

`oled_worker` reads a snapshot of `display_state` under `display_lock`,
then renders outside the lock. The lock is held for microseconds; the
luma.oled I2C write takes ~5ms and is never inside the lock.

---

## Startup Sequence

```
main():
  1. init pigpio daemon connection
  2. arm BSC slave: pi.bsc_i2c(I2C_ADDR)
  3. clear TX FIFO: load NOT_READY + zeros
  4. register EVENT_BSC callback: on_bsc_event
  5. init luma.oled on /dev/i2c-1
  6. display "BOOT OK" on OLED
  7. start bus_worker thread (daemon=True)
  8. start oled_worker thread (daemon=True)
  9. [Phase 4+] start flask thread (daemon=True)
 10. main thread blocks: signal.pause()  ← keeps process alive
```

`daemon=True` on all threads: if the main thread exits (e.g. KeyboardInterrupt),
all threads are cleaned up automatically. No explicit join needed for normal
shutdown.

---

## File Structure

```
code/raspi-db-server/
  CLAUDE.md           — setup steps, design notes, DoD per phase
  src/
    main.py           — startup sequence, thread creation
    bus_slave.py      — pigpio bsc_i2c setup, EVENT_BSC callback, TX FIFO protocol
    db.py             — SQLite read/write, WAL mode (Phase 4+)
    web_server.py     — Flask transaction history at :5000 (Phase 4+)
    oled.py           — luma.oled SSD1306 driver, oled_worker thread
    protocol.py       — message type constants, JSON builders (mirrors MessageProtocol)
  schema/
    schema.sql        — accounts + transactions tables (Phase 4+)
```

---

## Correspondence to FreeRTOS Pattern

| FreeRTOS (MCU) | Python (RPi) |
|---|---|
| `xTaskCreate(busWorker, pri=3)` | `threading.Thread(target=bus_worker)` |
| `xTaskCreate(oledTask, pri=1)` | `threading.Thread(target=oled_worker)` |
| `xQueueCreate(8, 256)` | `queue.Queue(maxsize=8)` |
| `xQueueReceive(q, portMAX_DELAY)` | `bus_queue.get()` — blocks, no CPU |
| `xQueueSend(q, item, 0)` | `bus_queue.put_nowait(item)` |
| `xSemaphoreTake(displayMutex)` | `with display_lock:` |
| `xSemaphoreGive(displayMutex)` | *(automatic on `with` block exit)* |
| `vTaskDelay(pdMS_TO_TICKS(500))` | `time.sleep(0.5)` |
| ISR callback → semaphore | pigpio EVENT_BSC callback → `queue.put()` |

The key structural difference: the pigpio callback fires from the pigpio
daemon's internal C thread — outside the Python process's thread model
entirely. `queue.Queue` is thread-safe and is the correct handoff mechanism
between the callback and `bus_worker`.

---

## Known Constraints

**pigpio daemon required:** `pigpiod` must be running before the Python
service starts. Add to `/etc/rc.local` or use a systemd service.

**BSC slave TX FIFO one-shot:** Once the MCU reads the TX FIFO, the BSC
slave hardware clears it. The ready flag drops to 0x00 naturally after
the read. `clear_tx_fifo()` is called defensively at startup and between
transactions.

**No clock stretching control:** The MCU drives SCL and will clock out
whatever is in the TX FIFO immediately. The ready-flag polling loop on the
MCU side is the only mechanism that handles RPi processing latency.

**Single SQLite writer:** All writes go through `bus_worker`. This is
intentional — it eliminates write contention and matches the Phase 3
sequential transaction model. Phase 5 concurrency (if needed) would
require a dedicated DB task and a request queue, matching the MCU #3
`sdQueue` / `sdResultQueue` pattern from the original FreeRTOS design.