# System Design — Low-Level Mainframe Simulation

## 1. Overview

A simulation of a banking mainframe architecture using four ESP32-C3 microcontrollers and
a Raspberry Pi 3B+, each representing a distinct mainframe subsystem. The devices
communicate over a shared I2C bus, mirroring IBM z/OS channel subsystem architecture.
The system processes banking transactions — deposits, withdrawals, and transfers —
providing hands-on experience with distributed system design, job scheduling, persistent
storage, and failure handling.

## 2. Requirements

See docs/requirements.md.

## 3. Architecture

### Subsystem Map

| Device | Role | Mainframe Equivalent | I2C Address |
|--------|------|---------------------|-------------|
| MCU #1 | Master Console | System Console / Operator Interface | 0x08 |
| MCU #2 | Transaction Processor | Central Processor (CP) | 0x09 |
| RPi 3B+ | Database Controller | DASD Controller | 0x0A |
| MCU #4 | Job Scheduler | JES (Job Entry Subsystem) | 0x0B |
| MCU #5 | I/O Controller | Channel Subsystem | 0x0C |

> MCU #3 (ESP32-WROOM-32 DevKit) — retired. See ADR-011.

### Communication Topology

All devices share one I2C bus (400kHz Fast Mode) via a central hub breadboard.
ESP32-C3 MCUs are multi-master capable — any MCU can initiate a transmission. Pull-up
resistors (5kΩ) on SDA and SCL lines. All devices share a common GND reference.

The Raspberry Pi 3B+ participates as a **pure I2C slave**. It connects to the shared bus
hub via GPIO18/19 (BCM2837 BSC slave peripheral — hardware-backed, not bit-banging).
The RPi never generates a START condition on the bus. Responses are returned via
master-read: the requesting MCU remains bus master and clocks the response out of the
RPi's TX FIFO. This is a different participation model from the MCUs but is transparent
to the rest of the bus — address 0x0A behaves identically from the caller's perspective.

### Per-Device Bus Architecture

Each ESP32-C3 MCU runs two independent I2C buses:
- Hardware I2C (TwoWire(0)): GPIO8=SDA, GPIO9=SCL — shared inter-device bus
- Software I2C (U8g2): GPIO3=SDA, GPIO10=SCL — private OLED display

The Raspberry Pi 3B+ uses two separate hardware peripherals on the BCM2837 SoC
(System-on-Chip):
- BSC slave (pigpio bsc_i2c): GPIO18=SDA, GPIO19=SCL — shared inter-device bus
- BSC1 master (luma.oled): GPIO2=SDA, GPIO3=SCL — private OLED display

GPIO18/19 and GPIO2/3 are separate peripherals in silicon. They do not interfere with
each other and connect to separate physical wires. Only GPIO18/19 connects to the shared
bus hub.

### Physical Layout

T-shape arrangement on 30×30cm wood base:
- Vertical long BB (spine): RPi 3B+ + hub
- Horizontal long BB (base): MCU #4 + MCU #5
- Short BB left: MCU #1
- Short BB right: MCU #2

## 4. Data Flow

### Deposit Transaction

```
MCU #5 (I/O) → MCU #4 (Scheduler):   JOB_SUBMIT
MCU #4 → MCU #2 (Processor):         JOB_DISPATCH
MCU #2 → RPi (Database, 0x0A):       DB_READ  (I2C write transaction)
RPi processes request, loads response into TX FIFO
MCU #2 → RPi:                         master-read (polls ready byte, 256 bytes)
RPi → MCU #2:                         DB_READ_RESULT  (via TX FIFO)
MCU #2 → RPi:                         DB_WRITE  (I2C write transaction)
RPi processes write, updates SQLite, loads ACK into TX FIFO
MCU #2 → RPi:                         master-read (polls ready byte, 256 bytes)
RPi → MCU #2:                         DB_WRITE_ACK  (via TX FIFO)
MCU #2 → MCU #4:                      JOB_COMPLETE
MCU #4 → MCU #5:                      JOB_RESULT
MCU #1 logs all messages throughout
```

Note: DB_READ_RESULT and DB_WRITE_ACK are not pushed by the RPi. They are returned via
master-read — MCU #2 polls `requestFrom(0x0A, 256)` in a loop until byte[0] (the ready
flag) is `0x01`, or a 500ms timeout elapses. This uses the `sendAndReceive()` method
added to the SharedBus library (Phase 3). See ADR-011 for the full protocol.

### Heartbeat

```
MCU #1 → all slaves (broadcast):   HEARTBEAT
Each MCU → MCU #1:                 HEARTBEAT_ACK  (pushed response, existing pattern)
RPi → MCU #1:                      HEARTBEAT_ACK  (via master-read, 20ms fixed delay)
MCU #1: updates OLED with active subsystem count
MCU #1: flags any device that does not respond within timeout
```

HEARTBEAT to the RPi uses `sendAndReceive()` with a fixed 20ms delay before the single
read (no poll loop needed — HEARTBEAT_ACK requires no SQLite query).

### Balance Enquiry

```
MCU #5 (I/O) → MCU #4 (Scheduler):  JOB_SUBMIT (txnType: BALANCE)
MCU #4 → MCU #2 (Processor):        JOB_DISPATCH
MCU #2 → RPi:                        DB_READ  + master-read
RPi → MCU #2:                        DB_READ_RESULT (balance)
MCU #2 → MCU #4:                     JOB_COMPLETE (balance in result)
MCU #4 → MCU #5:                     JOB_RESULT (balance)
MCU #5: displays balance on OLED / web console
```
Note: BALANCE is read-only — no DB_WRITE step.

### Insufficient Funds

```
MCU #2 → RPi: DB_READ  + master-read
RPi → MCU #2: DB_READ_RESULT (balance < amount)
MCU #2 → MCU #4: JOB_COMPLETE (status: FAILED, reason: INSUFFICIENT_FUNDS)
MCU #4 → MCU #5: JOB_RESULT (failed)
```

## 5. Data Model

### RPi — SQLite Storage (Phase 4+)

The RPi stores all account data in a SQLite database (`mainframe.db`) on its own SD card.
SQLite operates in WAL mode (Write-Ahead Log — allows concurrent reads while a write is
in progress) so Flask can read while `bus_worker` is writing, without blocking.

**Note on money representation:** All monetary values are stored and transmitted as integer
cents (uint32_t). $100.00 = 10000. Float arithmetic is never used for money — see ADR-005.
Formatting for display ($100.00) is done at output time only.

**accounts table**
```sql
CREATE TABLE accounts (
    account_id TEXT PRIMARY KEY,   -- 8-digit string e.g. "12345678"
    balance    INTEGER NOT NULL    -- cents, never float
);
```

**transactions table**
```sql
CREATE TABLE transactions (
    id         TEXT PRIMARY KEY,   -- UUID v4 from MCU message mid field
    account_id TEXT NOT NULL,
    txn_type   INTEGER NOT NULL,   -- matches TxnType constants: 1=DEPOSIT 2=WITHDRAW 4=BALANCE
    amount     INTEGER NOT NULL,   -- cents
    new_bal    INTEGER NOT NULL,   -- balance after transaction
    status     TEXT NOT NULL,      -- 'PENDING' or 'COMMITTED'
    ts         DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

Write-ahead log pattern for DB_WRITE (all three steps in a single SQLite transaction —
atomic):
1. Insert transaction row with `status='PENDING'`
2. Update account balance
3. Update transaction row to `status='COMMITTED'`

On RPi boot, any `PENDING` rows that never reached `COMMITTED` indicate an interrupted
write. Phase 5 adds replay logic for these rows.

**Phase 3 note:** SQLite is deferred to Phase 4. In Phase 3, `bus_worker` returns
hardcoded stub responses for DB_READ and DB_WRITE. The protocol, thread structure, and TX
FIFO mechanism are all live in Phase 3 — only the database backend is stubbed.

**Account seeding:** The accounts table is pre-populated before first use. Runtime account
creation via MCU #1 serial console is a Phase 3 feature.

### MCU #4 — Job Queue (in memory)

```cpp
struct Job {
    uint16_t jobId;       // composite: (senderAddr << 8) | sequenceNum
    uint8_t  priority;    // 0=HIGH, 1=MEDIUM, 2=LOW
    uint8_t  status;      // QUEUED, DISPATCHED, COMPLETE, FAILED
    char     type[16];    // "DEPOSIT", "WITHDRAW", "TRANSFER", "BALANCE"
    char     account[9];  // 8-digit account number + null terminator
    uint32_t amount;      // cents — never float
    uint32_t submittedAt;
    uint32_t dispatchedAt;
};
```

## 6. Interface Definitions

See docs/design/message_protocol.md.

## 7. Error Handling

### Subsystem Failure
- MCU #1 sends HEARTBEAT periodically to all devices including RPi
- Any device not responding within timeout is flagged on OLED
- Pending jobs for a failed subsystem are marked FAILED after timeout

### Message Timeout
- Sender waits max 500ms for response
- On timeout: logs error to MCU #1, returns BusError::TIMEOUT
- For RPi interactions: `sendAndReceive()` returns Status::TIMEOUT after 500ms poll
- MCU #4 retries dispatched jobs once before marking FAILED

### Malformed Message
- Receiver fails to parse JSON → logs error, discards message
- No retry — sender must detect via timeout

### Insufficient Funds
- MCU #2 detects during DB_READ_RESULT processing
- Returns JOB_COMPLETE with status FAILED and reason field
- No retry — this is a business logic rejection, not a system error

## 8. Open Questions

- [ ] RTC timestamp source for transaction log — DS1307 on MCU #1?
- [ ] How should MCU #1 web dashboard be structured (Phase 4)?
- [ ] Should MCU #4 persist the job queue to survive power loss?
- [ ] Maximum queue depth for MCU #4 before rejecting new jobs?
- [ ] Two-phase commit protocol design for TRANSFER (Phase 5)
- [ ] MCU #5 web console design — HTML form for DEPOSIT/WITHDRAW/BALANCE/TRANSFER
- [ ] WiFi + I2C simultaneous operation stability on ESP32-C3 (risk: timing interference)
- [ ] Runtime account creation via MCU #1 serial console (Phase 3 feature)
- [ ] Account administration — who can create/delete accounts, balance limits?
- [ ] BSC slave TX FIFO behaviour under rapid sequential requests — needs validation on
      real hardware in Phase 3 (clear_tx_fifo() between transactions assumed sufficient)