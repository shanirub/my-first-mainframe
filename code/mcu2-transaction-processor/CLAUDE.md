# MCU #2 — Transaction Processor

## Identity
- I2C address on shared bus: 0x09
- Role: validates and executes banking transactions, coordinates with MCU #3
- upload_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B0:C9:CC-if00
- monitor_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B0:C9:CC-if00

## Current State (Phase 3 — heartbeat scaffold running)
- FreeRTOS tasks running: Receiver (pri=3), Logic (pri=2), OLED (pri=1)
- Receiving HEARTBEAT from MCU #1 and sending HEARTBEAT_ACK correctly
- Transaction handling (JOB_DISPATCH logic) not yet implemented
- SharedBus v2 in use: sharedBus.init(I2C_ADDRESS)
- OLED confirmed working simultaneously with shared bus
- vTaskDelete(NULL) in loop() — do not add vTaskStartScheduler()

## Phase 3 Tasks
| Task | Priority | Stack | Role |
|---|---|---|---|
| Receiver | 3 | STACK_SIZE_RECEIVER | Blocks on rxSemaphore, puts messages on inboundQueue |
| Logic | 2 | STACK_SIZE_LOGIC | Sequential transaction handling (Option A) |
| OLED | 1 | STACK_SIZE_OLED | Updates display every 500ms under displayMutex |

## Phase 3 Logic — Sequential (Option A)
One transaction at a time. Logic task blocks while waiting for DB responses.

```
receive JOB_DISPATCH from MCU #4
→ send DB_READ to MCU #3
→ block on inboundQueue waiting for DB_READ_RESULT
→ validate (check funds if WITHDRAW, skip DB_WRITE if BALANCE)
→ send DB_WRITE to MCU #3
→ block on inboundQueue waiting for DB_WRITE_ACK
→ send JOB_COMPLETE to MCU #4
```

## Phase 4 Logic — State Machine (Option B)
Replace sequential logic with explicit state machine — never blocks mid-transaction.
Enables concurrent transactions. Surrounding code (receiver, OLED, queues) unchanged.

```cpp
enum class TxnState { IDLE, WAITING_DB_READ, WAITING_DB_WRITE };
```

## Display State
```cpp
struct DisplayState {
    uint32_t txnCount;
    char     lastTxnType[16];
    char     lastStatus[16];
};
```

## Critical Fixes (preserved for reference)
- sharedBus.begin() slave overload requires 4 args: begin(addr, sda, scl, 0)
  The frequency argument (0) is mandatory — no default. Without it, slave never responds.
- TwoWire(1) silently fails on ESP32-C3 (SOC_I2C_NUM=1). OLED uses U8g2 SW_I2C.
