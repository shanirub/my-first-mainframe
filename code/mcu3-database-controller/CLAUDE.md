# MCU #3 — Database Controller

## Identity
- I2C address on shared bus: 0x0A
- Role: account storage — SD card read/write, balance queries, transaction log
- upload_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:AF:41:D4-if00
- monitor_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:AF:41:D4-if00

## Current State (Phase 3 — heartbeat scaffold running)
- FreeRTOS tasks running: Receiver (pri=3), Logic (pri=2), OLED (pri=1)
- Receiving HEARTBEAT from MCU #1 and sending HEARTBEAT_ACK correctly
- SD card task and DB_READ/DB_WRITE logic not yet implemented
- vTaskDelete(NULL) in loop() — do not add vTaskStartScheduler()

## Phase 3 Tasks
| Task | Priority | Stack | Role |
|---|---|---|---|
| Receiver | 3 | STACK_SIZE_RECEIVER | Blocks on rxSemaphore, puts messages on inboundQueue |
| Logic | 2 | STACK_SIZE_LOGIC | Handles HEARTBEAT→ACK; Phase 3: DB_READ + DB_WRITE |
| SD | 2 | STACK_SIZE_LOGIC | Dedicated SD card task (accounts.json + transactions.log) |
| OLED | 1 | STACK_SIZE_OLED | Updates display every 500ms under displayMutex |

## Phase 3 Logic
```
receive DB_READ from MCU #2
→ SD task: read accounts.json, find account by ID
→ reply DB_READ_RESULT to MCU #2

receive DB_WRITE from MCU #2
→ SD task: append to transactions.log first (write-ahead)
→ SD task: update accounts.json
→ reply DB_WRITE_ACK to MCU #2
```

## Display State
```cpp
struct DisplayState {
    uint32_t rxCount;
    char     lastMsgType[16];
    char     lastError[24];
};
```

## Phase 4 Additions
- Crash recovery: on boot, check transactions.log for incomplete entries and replay
