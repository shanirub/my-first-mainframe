# MessageProtocol — API Reference

Library location: `code/shared/libs/message_protocol/`

This library handles all inter-MCU message construction, serialization, parsing,
and validation. It is used identically on all five MCUs. No instance is needed —
all methods are static.

---

## Quick start

```cpp
#include "message_protocol.h"

// Send a deposit job
JsonDocument doc = MessageProtocol::createEnvelope(
    ADDR_IO_CONTROLLER, ADDR_JOB_SCHEDULER, MsgType::JOB_SUBMIT
);
doc[Field::PAYLOAD][Field::TXN_TYPE] = TxnType::DEPOSIT;
doc[Field::PAYLOAD][Field::ACCOUNT]  = "12345678";
doc[Field::PAYLOAD][Field::AMOUNT]   = 10000;   // cents — $100.00
doc[Field::PAYLOAD][Field::PRIORITY] = Priority::MEDIUM;

char buf[256];
if (MessageProtocol::serialize(doc, buf, sizeof(buf))) {
    sharedBus.send(ADDR_JOB_SCHEDULER, buf);
}

// Receive and handle
char buf[256];
if (sharedBus.poll(buf, sizeof(buf))) {
    JsonDocument doc     = MessageProtocol::parse(buf);
    ValidationResult res = MessageProtocol::validate(doc);
    if (!res.valid) {
        Serial.println(res.detail);
        return;
    }
    uint8_t type = doc[Field::TYPE];
    // handle by type...
}
```

---

## Namespaces

### `Field` — wire-format key constants

All JSON field names. Always use these constants — never raw strings.

| Constant | Wire value | Used in |
|----------|-----------|---------|
| `Field::MSG_ID` | `"mid"` | every message |
| `Field::FROM` | `"f"` | every message |
| `Field::TO` | `"t"` | every message |
| `Field::TYPE` | `"tp"` | every message |
| `Field::PAYLOAD` | `"p"` | every message |
| `Field::JOB_ID` | `"jid"` | job messages |
| `Field::TXN_TYPE` | `"tt"` | JOB_SUBMIT, JOB_DISPATCH, DB_WRITE |
| `Field::ACCOUNT` | `"ac"` | transaction messages |
| `Field::AMOUNT` | `"am"` | transaction messages |
| `Field::BALANCE` | `"bal"` | DB_READ_RESULT, JOB_COMPLETE (BALANCE) |
| `Field::NEW_BAL` | `"nb"` | DB_WRITE |
| `Field::PRIORITY` | `"pr"` | JOB_SUBMIT |
| `Field::STATUS` | `"st"` | result messages |
| `Field::REASON` | `"rs"` | JOB_COMPLETE, JOB_RESULT on failure |
| `Field::TIMESTAMP` | `"ts"` | HEARTBEAT |
| `Field::UPTIME` | `"up"` | HEARTBEAT_ACK |
| `Field::CODE` | `"cd"` | ERROR |
| `Field::DETAIL` | `"dt"` | ERROR |

---

### `MsgType` — message type identifiers (`uint8_t`)

| Constant | Value | Direction |
|----------|-------|-----------|
| `MsgType::JOB_SUBMIT` | 1 | MCU #5 → MCU #4 |
| `MsgType::JOB_DISPATCH` | 2 | MCU #4 → MCU #2 |
| `MsgType::JOB_COMPLETE` | 3 | MCU #2 → MCU #4 |
| `MsgType::JOB_RESULT` | 4 | MCU #4 → MCU #5 |
| `MsgType::DB_READ` | 5 | MCU #2 → MCU #3 |
| `MsgType::DB_READ_RESULT` | 6 | MCU #3 → MCU #2 |
| `MsgType::DB_WRITE` | 7 | MCU #2 → MCU #3 |
| `MsgType::DB_WRITE_ACK` | 8 | MCU #3 → MCU #2 |
| `MsgType::HEARTBEAT` | 9 | MCU #1 → all (broadcast) |
| `MsgType::HEARTBEAT_ACK` | 10 | any → MCU #1 |
| `MsgType::ERROR` | 11 | any → any |

---

### `TxnType` — transaction types (`uint8_t`)

| Constant | Value | Notes |
|----------|-------|-------|
| `TxnType::DEPOSIT` | 1 | increases balance |
| `TxnType::WITHDRAW` | 2 | decreases balance, can fail |
| `TxnType::TRANSFER` | 3 | Phase 5 — two-phase commit |
| `TxnType::BALANCE` | 4 | read-only, no DB_WRITE step |

---

### `Status` — status and error codes (`uint8_t`)

| Constant | Value | Meaning |
|----------|-------|---------|
| `Status::OK` | 0 | operation succeeded |
| `Status::SUCCESS` | 1 | job completed successfully |
| `Status::FAILED` | 2 | job failed — see reason field |
| `Status::NOT_FOUND` | 3 | account does not exist |
| `Status::INSUFFICIENT_FUNDS` | 4 | balance < amount |
| `Status::DB_ERROR` | 5 | SD card read/write failure |
| `Status::TIMEOUT` | 6 | no response within 500ms |
| `Status::QUEUE_FULL` | 7 | MCU #4 queue at capacity |
| `Status::PARSE_ERROR` | 8 | received malformed JSON |
| `Status::SCHEMA_ERROR` | 9 | required field missing |

---

### `Priority` — job priority levels (`uint8_t`)

| Constant | Value | Default for |
|----------|-------|-------------|
| `Priority::HIGH` | 0 | TRANSFER |
| `Priority::MEDIUM` | 1 | DEPOSIT, WITHDRAW |
| `Priority::LOW` | 2 | BALANCE |

---

## Class: `MessageProtocol`

All methods are static. No instance required.

---

### `generateUUID`

```cpp
static void generateUUID(char* buf, size_t bufLen);
```

Generates a UUID v4 string using the ESP32 hardware RNG (`esp_random()`).

| Parameter | Type | Description |
|-----------|------|-------------|
| `buf` | `char*` | output buffer, must be ≥ 37 bytes |
| `bufLen` | `size_t` | size of buf |

Output format: `xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx` (RFC 4122).

Note: called automatically by `createEnvelope()` — you rarely need to call this directly.

---

### `createEnvelope`

```cpp
static JsonDocument createEnvelope(uint8_t from, uint8_t to, uint8_t type);
```

Creates a message envelope with a generated UUID. Payload object is created but empty — populate it after this call.

| Parameter | Type | Description |
|-----------|------|-------------|
| `from` | `uint8_t` | sender address — use `ADDR_*` constants from `shared_config.h` |
| `to` | `uint8_t` | recipient address, or `0` for broadcast |
| `type` | `uint8_t` | message type — use `MsgType::` constants |

Returns `JsonDocument` with `msgId`, `from`, `to`, `type`, and empty `payload`.

Example:
```cpp
JsonDocument doc = MessageProtocol::createEnvelope(
    ADDR_IO_CONTROLLER,
    ADDR_JOB_SCHEDULER,
    MsgType::JOB_SUBMIT
);
// Now add payload fields:
doc[Field::PAYLOAD][Field::ACCOUNT] = "12345678";
```

---

### `serialize`

```cpp
static bool serialize(JsonDocument& doc, char* buf, size_t bufLen);
```

Serializes a JsonDocument to a char buffer for transmission via `SharedBus::send()`.

| Parameter | Type | Description |
|-----------|------|-------------|
| `doc` | `JsonDocument&` | message to serialize |
| `buf` | `char*` | output buffer |
| `bufLen` | `size_t` | buffer size — use `256` (I2C buffer limit) |

Returns `true` on success. Returns `false` and logs to Serial if message exceeds `bufLen`. Message is dropped, never truncated.

---

### `parse`

```cpp
static JsonDocument parse(const char* buf);
```

Parses a raw buffer received from `SharedBus::poll()`.

Returns a populated `JsonDocument` on success. Returns an empty `JsonDocument` on failure — check with `doc.isNull()` before using.

Logs parse errors to Serial automatically.

---

### `validate`

```cpp
static ValidationResult validate(const JsonDocument& doc);
```

Validates envelope fields and payload fields against the schema registry. Strict — rejects if any required field is absent.

Returns `ValidationResult`:

```cpp
struct ValidationResult {
    bool    valid;       // true = message is valid
    uint8_t errorCode;  // Status:: constant on failure
    char    detail[64]; // human-readable reason on failure
};
```

Validation order:
1. Envelope fields (`msgId`, `from`, `to`, `type`, `payload`)
2. Message type recognised
3. Payload fields for that message type

---

### `msgTypeName` / `statusName` / `txnTypeName` / `priorityName`

```cpp
static const char* msgTypeName(uint8_t type);
static const char* statusName(uint8_t status);
static const char* txnTypeName(uint8_t txnType);
static const char* priorityName(uint8_t priority);
```

Convert integer codes to human-readable strings for Serial output.

**Do not use return values in logic comparisons** — compare against the `MsgType::` / `Status::` constants directly.

```cpp
// Correct — for display only:
Serial.printf("Received: %s\n", MessageProtocol::msgTypeName(type));

// Wrong — don't do this:
if (MessageProtocol::msgTypeName(type) == "JOB_SUBMIT") { ... }

// Correct — for logic:
if (type == MsgType::JOB_SUBMIT) { ... }
```

---

## Schema registry

Required payload fields per message type. Validation is strict — all listed fields must be present.

| Message type | Required payload fields |
|-------------|------------------------|
| `JOB_SUBMIT` | `tt`, `ac`, `am`, `pr` |
| `JOB_DISPATCH` | `jid`, `tt`, `ac`, `am` |
| `JOB_COMPLETE` | `jid`, `st` |
| `JOB_RESULT` | `jid`, `st` |
| `DB_READ` | `ac` |
| `DB_READ_RESULT` | `ac`, `bal`, `st` |
| `DB_WRITE` | `ac`, `am`, `nb`, `tt` |
| `DB_WRITE_ACK` | `ac`, `st` |
| `HEARTBEAT` | `ts` |
| `HEARTBEAT_ACK` | `st`, `up` |
| `ERROR` | `cd`, `dt` |

---

## Wire format example

Deposit job submission — 111 bytes:

```json
{"mid":"550e8400-e29b-41d4-a716-446655440000","f":12,"t":11,"tp":1,"p":{"tt":1,"ac":"12345678","am":10000,"pr":1}}
```

All monetary values (`am`, `bal`, `nb`) are integer cents. `10000` = $100.00. Float arithmetic is never used for money.

---

## Memory notes

- `_schemas` array lives in flash (declared `const`) — zero RAM cost
- `Field::` constants are `constexpr` — inlined at compile time, zero RAM cost
- `JsonDocument` is stack-allocated per call — freed automatically on return
- UUID buffer: 37 bytes stack-allocated in `generateUUID()`
- Message buffer: 256 bytes stack-allocated in caller's `main.cpp`
