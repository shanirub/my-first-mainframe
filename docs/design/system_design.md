# System Design — Low-Level Mainframe Simulation

## 1. Overview

A simulation of a banking mainframe architecture using five ESP32-C3
microcontrollers, each representing a distinct mainframe subsystem. The MCUs
communicate over a shared I2C bus, mirroring IBM z/OS channel subsystem
architecture. The system processes banking transactions — deposits,
withdrawals, and transfers — providing hands-on experience with distributed
system design, job scheduling, persistent storage, and failure handling.

## 2. Requirements

See docs/requirements.md.

## 3. Architecture

### Subsystem Map

| MCU | Role | Mainframe Equivalent | I2C Address |
|-----|------|---------------------|-------------|
| #1 | Master Console | System Console / Operator Interface | 0x08 |
| #2 | Transaction Processor | Central Processor (CP) | 0x09 |
| #3 | Database Controller | DASD Controller | 0x0A |
| #4 | Job Scheduler | JES (Job Entry Subsystem) | 0x0B |
| #5 | I/O Controller | Channel Subsystem | 0x0C |

### Communication Topology

All MCUs share one I2C bus (400kHz Fast Mode) via a central hub breadboard.
Any MCU can initiate a transmission (multi-master capable). Pull-up resistors
(5kΩ) on SDA and SCL lines. All MCUs share a common GND reference.

### Per-MCU Bus Architecture

Each MCU runs two independent I2C buses:
- Hardware I2C (TwoWire(0)): GPIO8=SDA, GPIO9=SCL — shared inter-MCU bus
- Software I2C (U8g2): GPIO3=SDA, GPIO10=SCL — private OLED display

### Physical Layout

T-shape arrangement on 30×30cm wood base:
- Vertical long BB (spine): MCU #3 + hub
- Horizontal long BB (base): MCU #4 + MCU #5
- Short BB left: MCU #1
- Short BB right: MCU #2

## 4. Data Flow

### Deposit Transaction

```
MCU #5 (I/O) → MCU #4 (Scheduler): JOB_SUBMIT
MCU #4 → MCU #2 (Processor):        JOB_DISPATCH
MCU #2 → MCU #3 (Database):         DB_READ (get balance)
MCU #3 → MCU #2:                     DB_READ_RESULT
MCU #2 → MCU #3:                     DB_WRITE (new balance)
MCU #3 → MCU #2:                     DB_WRITE_ACK
MCU #2 → MCU #4:                     JOB_COMPLETE
MCU #4 → MCU #5:                     JOB_RESULT
MCU #1 logs all messages throughout
```

### Heartbeat

```
MCU #1 → all (broadcast):   HEARTBEAT
Each MCU → MCU #1:          HEARTBEAT_ACK
MCU #1: updates OLED with active subsystem count
MCU #1: flags any MCU that does not respond within 1 second
```

### Balance Enquiry

```
MCU #5 (I/O) → MCU #4 (Scheduler): JOB_SUBMIT (txnType: BALANCE)
MCU #4 → MCU #2 (Processor):        JOB_DISPATCH
MCU #2 → MCU #3 (Database):         DB_READ
MCU #3 → MCU #2:                     DB_READ_RESULT (balance)
MCU #2 → MCU #4:                     JOB_COMPLETE (balance in result)
MCU #4 → MCU #5:                     JOB_RESULT (balance)
MCU #5: displays balance on OLED / web console
```
Note: BALANCE is read-only — no DB_WRITE step.

### Insufficient Funds

```
MCU #2 → MCU #3: DB_READ
MCU #3 → MCU #2: DB_READ_RESULT (balance < amount)
MCU #2 → MCU #4: JOB_COMPLETE (status: FAILED, reason: INSUFFICIENT_FUNDS)
MCU #4 → MCU #5: JOB_RESULT (failed)
```

## 5. Data Model

### MCU #3 — SD Card Storage

Two files maintained separately (dual-write pattern):

**Note on money representation:** All monetary values are stored and
transmitted as integer cents (uint32_t). $100.00 = 10000. Float arithmetic
is never used for money — see ADR-005. Formatting for display ($100.00)
is done at output time only.

**accounts.json**
```json
{
  "accounts": [
    { "id": "12345678", "balance": 100000, "owner": "Test User A" },
    { "id": "87654321", "balance": 50000,  "owner": "Test User B" }
  ]
}
```
Balances in cents: 100000 = $1000.00, 50000 = $500.00

**transactions.log** (append-only, one entry per line)
```
{"ts":1234567890,"jobId":"0C01","type":"DEPOSIT","account":"12345678","amount":10000,"status":"COMPLETE"}
{"ts":1234567891,"jobId":"0C02","type":"WITHDRAW","account":"87654321","amount":5000,"status":"FAILED","reason":"INSUFFICIENT_FUNDS"}
```
Amounts in cents. jobId format: sender address (hex) + sequence number.

**Account seeding:** accounts.json is pre-populated before first boot.
MCU #3 reads it on startup. If file not found, MCU #3 creates it with
default test accounts. Runtime account creation via MCU #1 serial console
is a Phase 3 feature.

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
- MCU #1 sends HEARTBEAT every 5 seconds
- Any MCU not responding within 1 second is flagged on OLED
- Pending jobs for a failed subsystem are marked FAILED after timeout

### Message Timeout
- Sender waits max 500ms for response
- On timeout: logs error to MCU #1, returns BusError::TIMEOUT
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
- [ ] How should MCU #1 web dashboard be structured (Phase 3)?
- [ ] Should MCU #4 persist the job queue to survive power loss?
- [ ] Maximum queue depth for MCU #4 before rejecting new jobs?
- [ ] Two-phase commit protocol design for TRANSFER (Phase 5)
- [ ] MCU #5 web console design — HTML form for DEPOSIT/WITHDRAW/BALANCE/TRANSFER
- [ ] WiFi + I2C simultaneous operation stability on ESP32-C3 (risk: timing interference)
- [ ] Runtime account creation via MCU #1 serial console (Phase 3 feature)
- [ ] Account administration — who can create/delete accounts, balance limits?
