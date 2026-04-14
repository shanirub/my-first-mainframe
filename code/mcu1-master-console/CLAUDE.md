# MCU #1 — Master Console

## Identity
- I2C address on shared bus: 0x08
- Role: system coordinator, heartbeat broadcaster, operator interface
- upload_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B1:F1:74-if00
- monitor_port = /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_10:00:3B:B1:F1:74-if00

## Current State (Phase 2.5 complete)
- FreeRTOS task pattern running and validated
- Tasks: Sender A (2s), Sender B (3s), Receiver (pri=3), Logic (pri=2), OLED (pri=1)
- Note: Sender A / Sender B were PoC-only (mutex stress test). Phase 3 replaces
  with a single Heartbeat task sending to all 4 slaves sequentially.
- SharedBus v2 in use: sharedBus.init(I2C_ADDRESS)
- busMutex and displayMutex both working
- OLED confirmed working simultaneously with shared bus
- vTaskDelete(NULL) in loop() — do not add vTaskStartScheduler()

## Phase 3 Tasks
| Task | Priority | Stack | Role |
|---|---|---|---|
| Receiver | 3 | STACK_SIZE_RECEIVER | Blocks on rxSemaphore, puts messages on inboundQueue |
| Heartbeat | 2 | STACK_SIZE_SENDER | Sends HEARTBEAT to all 4 slaves every 5s, tracks responses |
| Serial input | 2 | STACK_SIZE_LOGIC | Reads USB serial, parses commands, puts on serialQueue |
| Logic | 2 | STACK_SIZE_LOGIC | Drains inboundQueue + serialQueue, handles ACKs and errors |
| OLED | 1 | STACK_SIZE_OLED | Updates display every 500ms under displayMutex |

## Serial Command Format (Phase 3)
Commands typed in PlatformIO serial monitor, newline-terminated:
```
DEPOSIT 12345678 10000     ← account number, amount in cents
WITHDRAW 12345678 5000
BALANCE 12345678
STATUS                     ← print subsystem health to serial
```

## Display State
```cpp
struct DisplayState {
    uint8_t  activeSubsystems;   // count of MCUs that ACKed last heartbeat
    uint32_t heartbeatCount;
    char     lastError[24];
};
```

## Phase 4 Additions
- WiFi web dashboard replaces serial monitor for operator commands
- RTC (DS1307) timestamps on all log entries
