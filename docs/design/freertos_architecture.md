# FreeRTOS Architecture — All Five MCUs

## Overview

Each MCU runs a set of FreeRTOS tasks rather than a single Arduino loop().
All work happens in tasks. `setup()` initialises hardware and creates tasks.
`loop()` calls `vTaskDelete(NULL)` to reclaim the loopTask stack and returns.

The shared bus is protected by a mutex inside SharedBus. Any task on any MCU
can call `sharedBus.send()` — the mutex ensures only one transmission happens
at a time on that MCU. I2C hardware arbitration handles the case where two
different MCUs transmit simultaneously.

**Validated by PoC (2026-04-13):** mode switching, queue integrity, and mutex
protection all confirmed working on real hardware. See `docs/poc_rtos/results.md`.

---

## Shared task pattern

Every MCU uses this base pattern, extended with subsystem-specific tasks:

| Task | Priority | Stack | Role |
|---|---|---|---|
| Receiver | 3 (highest) | 2048 | Blocks on rxSemaphore, puts raw buffer on inboundQueue |
| Logic | 2 | 4096 | Drains inboundQueue, handles messages, sends responses |
| OLED | 1 (lowest) | 2048 | Reads displayState under mutex, updates display every 500ms |

**Display state** is a small struct protected by a dedicated `displayMutex`
(separate from `busMutex` — the bus can be held for tens of milliseconds,
the display mutex is held for microseconds).

**Inbound queue depth:** 8 messages on all MCUs. Each slot is 256 bytes
(matching I2C_BUFFER_LENGTH). Total per MCU: 2KB queue RAM.

---

## MCU #1 — Master Console (0x08)

### Responsibilities
- Broadcast HEARTBEAT to all four slaves every 5 seconds
- Detect and display non-responding subsystems
- Accept operator commands via USB serial
- Log ERROR messages received from any MCU

### Tasks

| Task | Priority | Stack | Phase |
|---|---|---|---|
| Receiver | 3 | 2048 | 3 |
| Heartbeat | 2 | 4096 | 3 |
| Serial input | 2 | 4096 | 3 |
| Logic | 2 | 4096 | 3 |
| OLED | 1 | 2048 | 3 |

### Internal queues

| Queue | Producer | Consumer | Depth | Purpose |
|---|---|---|---|---|
| inboundQueue | Receiver | Logic | 8 | incoming ACKs and ERROR messages |
| serialQueue | Serial input | Logic | 4 | parsed operator commands |

### Serial command format (Phase 3)
Commands typed in serial monitor, one per line:

```
DEPOSIT 12345678 10000       ← account, amount in cents
WITHDRAW 12345678 5000
BALANCE 12345678
STATUS                       ← print subsystem health to serial
```

Serial input task reads `Serial.readStringUntil('\n')`, tokenises, puts a
command struct on `serialQueue`. Logic task drains both `inboundQueue` and
`serialQueue`.

### Display state
```cpp
struct DisplayState {
    uint8_t  activeSubsystems;   // count of MCUs that ACKed last heartbeat
    uint32_t heartbeatCount;
    char     lastError[24];      // last ERROR message received
};
```

### OLED layout
```
MASTER CONSOLE
HB: 42  Active: 4/4
Last err: none
Uptime: 3600s
```

### Phase 3 / Phase 4 split
Phase 3: heartbeat sends to all four slaves sequentially, waits for ACK,
marks non-responders. One serial command processed per heartbeat cycle.

Phase 4: add WiFi web dashboard replacing serial monitor for operator commands.
Add RTC timestamp to all log entries.

---

## MCU #2 — Transaction Processor (0x09)

### Responsibilities
- Receive JOB_DISPATCH from MCU #4
- Read account balance from MCU #3 (DB_READ → DB_READ_RESULT)
- Validate transaction (sufficient funds for withdrawal)
- Write new balance to MCU #3 (DB_WRITE → DB_WRITE_ACK)
- Report result to MCU #4 (JOB_COMPLETE)

### Tasks

| Task | Priority | Stack | Phase |
|---|---|---|---|
| Receiver | 3 | 2048 | 3 |
| Logic | 2 | 4096 | 3 |
| OLED | 1 | 2048 | 3 |

### Internal queues

| Queue | Producer | Consumer | Depth | Purpose |
|---|---|---|---|---|
| inboundQueue | Receiver | Logic | 8 | all incoming messages |

### Phase 3 implementation — sequential (Option A)

The logic task handles one transaction at a time. It blocks while waiting
for DB responses. This is safe in Phase 3 because only one transaction
flows through the system at a time.

```
Logic task state machine (Phase 3 — sequential):

receive JOB_DISPATCH
  → send DB_READ to MCU #3
  → block on inboundQueue waiting for DB_READ_RESULT
  → validate (check funds if WITHDRAW)
  → send DB_WRITE to MCU #3  (skipped for BALANCE)
  → block on inboundQueue waiting for DB_WRITE_ACK
  → send JOB_COMPLETE to MCU #4
  → ready for next job
```

### Phase 4 implementation — state machine (Option B)

Replace the sequential logic with an explicit state machine. The task never
blocks mid-transaction — each message advances the state and the task returns
to draining the queue immediately. Enables concurrent transactions.

```cpp
enum class TxnState { IDLE, WAITING_DB_READ, WAITING_DB_WRITE };
```

The surrounding code (receiver task, OLED task, SharedBus calls, queue
structure) is unchanged — only the inside of the logic task is replaced.

### Display state
```cpp
struct DisplayState {
    uint32_t txnCount;
    char     lastTxnType[16];
    char     lastStatus[16];
};
```

### OLED layout
```
TRANS PROCESSOR
Addr: 0x09
TXN: 42  Last: DEPOSIT
Status: SUCCESS
```

---

## MCU #3 — Database Controller (0x0A)

### Responsibilities
- Receive DB_READ, look up account on SD card, return DB_READ_RESULT
- Receive DB_WRITE, write new balance to SD card, return DB_WRITE_ACK
- Maintain two files: `accounts.json` (current balances) and `transactions.log` (append-only)
- Write-ahead log pattern: log entry written before balance update

### Tasks

| Task | Priority | Stack | Phase |
|---|---|---|---|
| Receiver | 3 | 2048 | 3 |
| Logic | 2 | 4096 | 3 |
| SD | 2 | 4096 | 3 |
| OLED | 1 | 2048 | 3 |

### Internal queues

| Queue | Producer | Consumer | Depth | Purpose |
|---|---|---|---|---|
| inboundQueue | Receiver | Logic | 8 | incoming DB_READ and DB_WRITE requests |
| sdQueue | Logic | SD task | 4 | SD operation requests |
| sdResultQueue | SD task | Logic | 4 | SD operation results |

### SD request struct
```cpp
struct SdRequest {
    uint8_t  operation;    // SD_READ or SD_WRITE
    uint8_t  replyTo;      // I2C address to send result back to
    char     account[9];
    uint32_t newBalance;   // only for SD_WRITE
    uint8_t  txnType;      // only for SD_WRITE, for log entry
    uint32_t amount;       // only for SD_WRITE, for log entry
};
```

### Why a dedicated SD task

SD card access via SPI takes 5–50ms. If the logic task did SD access directly
it would block the inbound queue from draining during that window. A dedicated
SD task owns all SD card access — only one task ever touches the card, so no
mutex is needed for SPI. The 5–50ms delay only blocks the SD task, not the
receiver or logic tasks.

### Write-ahead log pattern
On DB_WRITE, the SD task:
1. Appends to `transactions.log` first
2. Updates `accounts.json` second

On boot, MCU #3 checks if the last log entry has a matching balance update.
If not, the previous transaction was interrupted — replay it. (Phase 5 feature.)

### Display state
```cpp
struct DisplayState {
    uint32_t readCount;
    uint32_t writeCount;
    char     lastAccount[9];
    char     lastError[24];
};
```

### OLED layout
```
DATABASE CTRL
Addr: 0x0A
R:128 W:64
Last: 12345678
```

---

## MCU #4 — Job Scheduler (0x0B)

### Responsibilities
- Receive JOB_SUBMIT from MCU #5, assign jobId, queue job
- Dispatch next job to MCU #2 when MCU #2 is free
- Receive JOB_COMPLETE from MCU #2, mark job done, notify MCU #5
- Track job state: QUEUED → DISPATCHED → COMPLETE / FAILED

### Tasks

| Task | Priority | Stack | Phase |
|---|---|---|---|
| Receiver | 3 | 2048 | 3 |
| Logic | 2 | 4096 | 3 |
| OLED | 1 | 2048 | 3 |

### Internal queues

| Queue | Producer | Consumer | Depth | Purpose |
|---|---|---|---|---|
| inboundQueue | Receiver | Logic | 8 | JOB_SUBMIT and JOB_COMPLETE messages |

### Job struct (in-memory)
```cpp
struct Job {
    uint16_t jobId;          // composite: (senderAddr << 8) | sequenceNum
    uint8_t  priority;       // Priority::HI / MEDIUM / LO
    uint8_t  status;         // QUEUED, DISPATCHED, COMPLETE, FAILED
    uint8_t  txnType;        // TxnType constants
    char     account[9];
    uint32_t amount;         // cents
    uint8_t  submittedBy;    // I2C address of submitter (MCU #5)
    uint32_t submittedAt;    // millis()
};

Job jobQueue[MAX_JOBS];      // MAX_JOBS = 8, defined in shared_config.h
```

### Phase 3 implementation — immediate dispatch

On JOB_SUBMIT: assign jobId, store in jobQueue, immediately dispatch to
MCU #2 via JOB_DISPATCH. Mark job DISPATCHED.

On JOB_COMPLETE: find job by jobId, mark COMPLETE or FAILED, send JOB_RESULT
to submitter (MCU #5), remove from queue.

MCU #2 is assumed free when a new job arrives in Phase 3 (one transaction at
a time). No dispatch coordination needed.

### Phase 4 implementation — priority queue

Replace immediate dispatch with a priority queue. Jobs are sorted by
`Priority::HI > MEDIUM > LO`. MCU #4 tracks whether MCU #2 is busy
(DISPATCHED job exists). Dispatch only fires when MCU #2 is free. This
enables queuing multiple jobs and processing them in priority order.

The surrounding code (receiver, OLED, queue structure) is unchanged —
only the dispatch logic inside the logic task is replaced.

### Display state
```cpp
struct DisplayState {
    uint8_t  queueDepth;
    uint32_t totalDispatched;
    uint32_t totalFailed;
    char     lastJobType[16];
};
```

### OLED layout
```
JOB SCHEDULER
Addr: 0x0B
Queue: 2  Done: 41
Last: DEPOSIT
```

---

## MCU #5 — I/O Controller (0x0C)

### Responsibilities
- Host WiFi HTTP server for customer-facing web console
- Receive transaction submissions via HTTP POST, submit as JOB_SUBMIT to MCU #4
- Receive JOB_RESULT from MCU #4, return HTTP response to waiting browser
- Display last transaction result on OLED

### Tasks

| Task | Priority | Stack | Phase |
|---|---|---|---|
| Receiver | 3 | 2048 | 3 |
| Logic | 2 | 4096 | 3 |
| HTTP server | 2 | 8192 | 3 |
| OLED | 1 | 2048 | 3 |

Note: HTTP server task gets 8192 bytes — the ESP32 WiFi stack and HTTP
request parsing require significantly more stack than pure I2C tasks.

### Internal queues

| Queue | Producer | Consumer | Depth | Purpose |
|---|---|---|---|---|
| inboundQueue | Receiver | Logic | 8 | incoming JOB_RESULT messages |

### Pending request table

When the HTTP server receives a POST, it submits JOB_SUBMIT and needs to
hold the connection open until JOB_RESULT arrives asynchronously over I2C.
The pending request table bridges these two tasks:

```cpp
struct PendingRequest {
    uint16_t jobId;
    bool     resultReady;
    uint8_t  resultStatus;
    char     resultReason[32];
    uint32_t resultBalance;    // for BALANCE queries only
};

PendingRequest pendingTable[MAX_PENDING];   // MAX_PENDING = 4
SemaphoreHandle_t pendingTableMutex;
```

Flow:
1. HTTP task receives POST → builds JOB_SUBMIT → sends to MCU #4 → stores jobId in pendingTable → polls `resultReady` every 50ms with vTaskDelay
2. Logic task receives JOB_RESULT → finds matching jobId in pendingTable → sets `resultReady = true`, copies status and balance
3. HTTP task wakes on next poll → sends HTTP response → clears slot

If pendingTable is full: HTTP task returns HTTP 503.
If no JOB_RESULT arrives within 2 seconds: HTTP task returns HTTP 504 (timeout).

Both tasks access pendingTable under `pendingTableMutex`.

### Phase 3 — single pending slot

Phase 3 uses MAX_PENDING = 1. One transaction at a time. Simple and matches
the rest of the Phase 3 single-transaction assumption.

Phase 4 expands to MAX_PENDING = 4 alongside MCU #2 state machine and MCU #4
priority queue.

### Web console (Phase 3)
Simple HTML form served from flash (hardcoded string in firmware):

```
Deposit:   [account____] [amount____] [Submit]
Withdraw:  [account____] [amount____] [Submit]
Balance:   [account____] [Submit]
```

Result displayed on the same page after form submission.

### Display state
```cpp
struct DisplayState {
    uint32_t txnCount;
    char     lastTxnType[16];
    char     lastStatus[16];
    uint32_t lastBalance;      // cents, for BALANCE result
};
```

### OLED layout
```
I/O CONTROLLER
Addr: 0x0C
TXN: 17  Last: BALANCE
BAL: $500.00
```

---

## Task testing order (per MCU)

Each MCU is implemented and tested incrementally before moving to the next:

1. **Receiver + OLED** — flash, confirm messages arrive from MCU #1 heartbeat, OLED updates
2. **Logic task** — confirm correct response sent back (HEARTBEAT_ACK first, then subsystem messages)
3. **Subsystem task** — SD task (MCU #3), HTTP task (MCU #5) tested in isolation
4. **Full flow** — connect to live system, run end-to-end transaction

---

## FreeRTOS handle summary (per MCU)

All handles are globals in `main.cpp`:

```cpp
QueueHandle_t     inboundQueue;      // all MCUs
SemaphoreHandle_t displayMutex;      // all MCUs
// busMutex lives inside SharedBus, exposed as sharedBus.busMutex if needed

// MCU #3 only:
QueueHandle_t     sdQueue;
QueueHandle_t     sdResultQueue;

// MCU #5 only:
SemaphoreHandle_t pendingTableMutex;
```

---

## Known constraints

**Half-duplex timing:** A sender occasionally receives `BusError::NOT_FOUND`
when the target MCU is briefly in master mode sending its own response. This
is expected I2C half-duplex behavior. Handled in Phase 3 with one retry before
reporting failure.

**Single-core:** ESP32-C3 has one core. All tasks time-slice on that core.
True parallelism is not possible — but the task/queue pattern still provides
clean concurrency semantics and correct mutual exclusion.

**Stack overflow detection:** Enable `CONFIG_FREERTOS_USE_TRACE_FACILITIES`
and call `uxTaskGetStackHighWaterMark()` during development if unexpected
crashes occur. The stack sizes defined in `shared_config.h` are conservative
starting values.
