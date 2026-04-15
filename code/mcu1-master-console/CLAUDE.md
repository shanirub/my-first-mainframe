# MCU #1 — Master Console

## Identity
- I2C address on shared bus: 0x08
- Role: system coordinator, heartbeat broadcaster, operator interface
- upload_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B1:F1:74-if00
- monitor_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B1:F1:74-if00

## Current State (Phase 3 in progress)
- FreeRTOS tasks running: Receiver (pri=3), Heartbeat (pri=2), SerialIn (pri=2), Logic (pri=2), OLED (pri=1)
- Heartbeat confirmed: sends to all 4 slaves every HEARTBEAT_INTERVAL_MS (10s), ACKs tracked by timestamp
- OLED shows Act:X/4 — slave active if ACKed within HEARTBEAT_ACK_TIMEOUT_MS (30s)
- Serial input task and logic task exist; command dispatch (DEPOSIT/WITHDRAW/BALANCE → MCU #4) is still a stub
- SharedBus v2 in use: sharedBus.init(I2C_ADDRESS)
- busMutex and displayMutex both working
- vTaskDelete(NULL) in loop() — do not add vTaskStartScheduler()

## Phase 3 Tasks
| Task | Priority | Stack | Role |
|---|---|---|---|
| Receiver | 3 | STACK_SIZE_RECEIVER | Blocks on rxSemaphore, puts messages on inboundQueue |
| Heartbeat | 2 | STACK_SIZE_SENDER | Sends HEARTBEAT to all 4 slaves every HEARTBEAT_INTERVAL_MS, records ACK timestamps |
| Serial input | 2 | STACK_SIZE_LOGIC | Reads USB serial, parses commands, puts on serialQueue |
| Logic | 2 | STACK_SIZE_LOGIC | Drains inboundQueue + serialQueue, handles ACKs and errors; dispatch to MCU #4 pending |
| OLED | 1 | STACK_SIZE_OLED | Updates display every 500ms under displayMutex |

## Serial Command Format (Phase 3)
Commands typed in PlatformIO serial monitor, newline-terminated:
```
DEPOSIT 12345678 10000     ← account number, amount in cents
WITHDRAW 12345678 5000
BALANCE 12345678
STATUS                     ← print subsystem health to serial
```

## SharedState
```cpp
struct SharedState {
    uint32_t lastAckTime[4];  // indexed: 0=0x09, 1=0x0A, 2=0x0B, 3=0x0C
    uint32_t heartbeatCount;
    char     lastError[24];
};
```
Active count is computed from timestamps at display time — never stored as a field.
Constants in shared_config.h: HEARTBEAT_INTERVAL_MS=10000, HEARTBEAT_ACK_TIMEOUT_MS=30000.

## Phase 4 Additions
- WiFi web dashboard replaces serial monitor for operator commands
- RTC (DS1307) timestamps on all log entries
