# MCU #5 — I/O Controller

## Identity
- I2C address on shared bus: 0x0C
- Role: web console — WiFi HTTP server, accepts transaction requests, displays result
- upload_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:AF:5F:A4-if00
- monitor_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:AF:5F:A4-if00

## Current State (Phase 3 — heartbeat scaffold running)
- FreeRTOS tasks running: Receiver (pri=3), Logic (pri=2), OLED (pri=1)
- Receiving HEARTBEAT from MCU #1 and sending HEARTBEAT_ACK correctly
- WiFi/HTTP server and JOB_SUBMIT logic not yet implemented
- vTaskDelete(NULL) in loop() — do not add vTaskStartScheduler()

## Phase 3 Tasks
| Task | Priority | Stack | Role |
|---|---|---|---|
| Receiver | 3 | STACK_SIZE_RECEIVER | Blocks on rxSemaphore, puts messages on inboundQueue |
| HTTP Server | 2 | 8192 | WiFi AP + HTTP server, puts requests on httpQueue |
| Logic | 2 | STACK_SIZE_LOGIC | Drains httpQueue, sends JOB_SUBMIT to MCU #4, waits for result |
| OLED | 1 | STACK_SIZE_OLED | Updates display every 500ms under displayMutex |

## Phase 3 Logic
```
HTTP POST /transaction (DEPOSIT/WITHDRAW/BALANCE)
→ put request on httpQueue (single slot — reject if occupied)
→ logic task: send JOB_SUBMIT to MCU #4
→ wait for JOB_RESULT on inboundQueue
→ display result on OLED + return HTTP response
```
Single pending request slot. Expand to 4 in Phase 4.

## Display State
```cpp
struct DisplayState {
    uint32_t rxCount;
    char     lastMsgType[16];
    char     lastError[24];
};
```

## Phase 4 Additions
- Expand httpQueue to 4 concurrent pending requests
