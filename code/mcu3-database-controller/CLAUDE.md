# MCU #3 — Database Controller

## Identity
- I2C address on shared bus: 0x0A
- Role: account storage proxy — receives DB_READ/DB_WRITE on shared bus,
  forwards to Raspberry Pi 3B+ over UART, returns result on shared bus
- Board: ESP32-WROOM-32 DevKit (38-pin, CP2102, Type-C)
- upload_port = /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0
- monitor_port = /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0

## Architecture Note
MCU #3 does not store data directly. It is the shared bus participant at
0x0A and acts as a proxy between the bus and a Raspberry Pi 3B+ that owns
SQLite storage and the Flask web interface. Communication to RPi is over
UART1 (GPIO18/19) using newline-terminated JSON.

For full architecture reasoning and hardware debugging history, see:
- ADR-008: three board replacements, SD card exhaustion
- ADR-009: UART/RPi decision, protocol, RPi setup requirements

For RPi setup steps, Python script design, and SQLite schema, see:
- code/raspi-db-server/CLAUDE.md

## Hardware History
MCU #3 went through three board replacements before reaching this board.
See ADR-008 for the full account. Short version:
1. ESP32-C3 SuperMini — insufficient free GPIOs for shared bus + OLED + SPI
2. WeMos LOLIN32 Lite clone — GPIO8/9 tied to 32kHz crystal, not available as user GPIO
3. ESP32-WROOM-32 DevKit (current) — GPIO8/9 available, all pins clean

## Pin Configuration
| Function | GPIO | Notes |
|---|---|---|
| Shared bus SDA | GPIO8 | Matches all other MCUs — hub wiring unchanged |
| Shared bus SCL | GPIO9 | Matches all other MCUs — hub wiring unchanged |
| OLED SDA | GPIO16 | U8g2 SW I2C |
| OLED SCL | GPIO17 | U8g2 SW I2C |
| UART TX (to RPi) | GPIO18 | UART1 — RPi GPIO15 (RX) |
| UART RX (from RPi) | GPIO19 | UART1 — RPi GPIO14 (TX) |

GPIO5, GPIO23 are unconnected. SD card module is not used.

## UART Protocol
Newline-terminated JSON. MCU #3 writes a request and reads until `\n`.
Timeout: 500ms — on expiry, return `Status::TIMEOUT` to MCU #2.

Requests to RPi:
```json
{"op":"read","ac":"12345678"}
{"op":"write","ac":"12345678","nb":60000,"tt":1,"mid":"uuid"}
```

Responses from RPi:
```json
{"bal":50000,"st":0}
{"st":0}
```

## FreeRTOS Tasks
| Task | Priority | Stack | Role |
|---|---|---|---|
| Receiver | 3 | STACK_SIZE_RECEIVER | Blocks on rxSemaphore, puts messages on inboundQueue |
| Logic | 2 | STACK_SIZE_LOGIC | Handles HEARTBEAT→ACK, DB_READ, DB_WRITE |
| UART | 2 | STACK_SIZE_LOGIC | Owns Serial2 — sends requests to RPi, reads responses |
| OLED | 1 | STACK_SIZE_OLED | Updates display every 500ms under displayMutex |

## Internal Queues
| Queue | Producer | Consumer | Depth | Purpose |
|---|---|---|---|---|
| inboundQueue | Receiver | Logic | 8 | incoming DB_READ and DB_WRITE requests |
| uartQueue | Logic | UART task | 4 | UART operation requests to RPi |
| uartResultQueue | UART task | Logic | 4 | UART operation results from RPi |

## Logic Flow
```
receive DB_READ from MCU #2
→ put UartRequest on uartQueue (op=read, account=...)
→ block on uartResultQueue with 500ms timeout
→ on timeout: send DB_READ_RESULT with Status::TIMEOUT to MCU #2
→ on result: send DB_READ_RESULT with balance to MCU #2

receive DB_WRITE from MCU #2
→ put UartRequest on uartQueue (op=write, account=..., newBalance=..., txnType=..., mid=...)
→ block on uartResultQueue with 500ms timeout
→ on timeout: send DB_WRITE_ACK with Status::TIMEOUT to MCU #2
→ on result: send DB_WRITE_ACK with Status::OK to MCU #2
```

## SharedState
```cpp
struct SharedState {
    uint32_t readCount;
    uint32_t writeCount;
    char     lastAccount[9];
    char     lastError[24];
};
```

## OLED Layout
```
DATABASE CTRL
Addr: 0x0A
R:128 W:64
Last: 12345678
```

## Phase 3 Implementation Steps

### Step 1 — UART loopback test
Wire GPIO18→GPIO19 (TX→RX loopback). Flash a sketch that sends a string
on Serial2 and reads it back. No RPi needed.
DoD: loopback string received correctly, printed to Serial.

### Step 2 — RPi UART echo server
RPi runs a minimal Python script: read a line on serial, echo it back.
MCU #3 sends a test line, reads the echo.
DoD: MCU #3 serial shows sent string echoed back from RPi.

### Step 3 — FreeRTOS skeleton + heartbeat ACK
Wire shared bus (GPIO8/9) to hub. Flash FreeRTOS skeleton with Receiver,
Logic, OLED tasks. No UART task yet — logic task stubs DB_READ/DB_WRITE.
DoD: MCU #3 appears on 5-MCU bus test, heartbeat ACKs visible on MCU #1.

### Step 4 — UART task + RPi DB_READ handler
Implement UART task. RPi db_server.py handles `op=read` — queries SQLite,
returns balance JSON. MCU #3 logic task sends DB_READ to RPi via uartQueue,
returns DB_READ_RESULT to MCU #2.
DoD: MCU #2 receives correct balance for a known account.

### Step 5 — RPi DB_WRITE handler
RPi db_server.py handles `op=write` — write-ahead log entry, update balance,
mark committed. MCU #3 sends DB_WRITE_ACK to MCU #2.
DoD: balance updated in SQLite, transaction log entry present, ACK received by MCU #2.

### Step 6 — Timeout handling
Remove RPi from UART while MCU #3 is running. Confirm MCU #3 returns
Status::TIMEOUT to MCU #2 within 500ms rather than hanging.
DoD: MCU #2 receives DB_READ_RESULT with st=6 (TIMEOUT), no task hangs.

### Step 7 — End-to-end transaction flow
Full DEPOSIT and WITHDRAW via MCU #1 serial console → MCU #4 → MCU #2 →
MCU #3 → RPi → back up the chain.
DoD: balance changes correctly in SQLite, all ACKs propagate, MCU #1 shows
transaction complete.

## Critical Notes
- Do NOT call vTaskStartScheduler() — ESP32 Arduino already started FreeRTOS
- Call vTaskDelete(NULL) in loop() to reclaim loopTask stack
- sharedBus.init(I2C_ADDRESS) must be called before xTaskCreate()
- Serial2.begin() for UART1 — Serial is UART0 (USB), do not use for RPi
- RPi must be booted before MCU #3 starts serving DB requests — UART timeout
  handles RPi-down gracefully
- GPIO8/9 availability must be verified on any future board replacement —
  these are crystal pins on some boards (LOLIN32 Lite) and unavailable