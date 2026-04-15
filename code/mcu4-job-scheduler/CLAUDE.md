# MCU #4 — Job Scheduler

## Identity
- I2C address on shared bus: 0x0B
- Role: job dispatch — receives JOB_SUBMIT from MCU #1, forwards to MCU #2
- upload_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:AF:AB:94-if00
- monitor_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:AF:AB:94-if00

## Current State (Phase 3 — heartbeat scaffold running)
- FreeRTOS tasks running: Receiver (pri=3), Logic (pri=2), OLED (pri=1)
- Receiving HEARTBEAT from MCU #1 and sending HEARTBEAT_ACK correctly
- JOB_SUBMIT handling and JOB_DISPATCH logic not yet implemented
- vTaskDelete(NULL) in loop() — do not add vTaskStartScheduler()

## Phase 3 Tasks
| Task | Priority | Stack | Role |
|---|---|---|---|
| Receiver | 3 | STACK_SIZE_RECEIVER | Blocks on rxSemaphore, puts messages on inboundQueue |
| Logic | 2 | STACK_SIZE_LOGIC | Handles HEARTBEAT→ACK; Phase 3: JOB_SUBMIT→JOB_DISPATCH |
| OLED | 1 | STACK_SIZE_OLED | Updates display every 500ms under displayMutex |

## Phase 3 Logic
```
receive JOB_SUBMIT from MCU #1
→ immediately send JOB_DISPATCH to MCU #2
→ wait for JOB_COMPLETE from MCU #2
→ forward JOB_RESULT to MCU #1
```
One job at a time (immediate dispatch, no queue). Priority queue deferred to Phase 4.

## Display State
```cpp
struct DisplayState {
    uint32_t rxCount;
    char     lastMsgType[16];
    char     lastError[24];
};
```

## Phase 4 Additions
- Priority queue: HIGH/MEDIUM/LOW ordering for queued jobs
- Timeout + single retry before marking job FAILED and alerting MCU #1
