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
- ADR-010: platform switch to pioarduino, IDF 5.5.x, i2c_new_slave_device()

For RPi setup steps, Python script design, and SQLite schema, see:
- code/raspi-db-server/CLAUDE.md

For implementation status and next steps, see the project roadmap:
- docs/roadmap.md — MCU #3 phases 0–4 with checkboxes

## Platform
MCU #3 uses **pioarduino** — a community fork of the PlatformIO espressif32
platform that supports arduino-esp32 3.x (IDF 5.5.x). The official PlatformIO
platform (espressif32@7.0.1) bundles IDF 4.4.7, which does not have
i2c_new_slave_device(). Both Arduino Wire slave and i2c_driver_install() in
slave mode caused TG1WDT resets on this board. The Wire path root cause is
confirmed by source code — see ADR-010 for full investigation and decision
chain. i2c_new_slave_device() (IDF 5.2+) is interrupt-driven and creates no
internal task — this is the correct fix.

platformio.ini platform line:
```
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
```

Other MCUs (#1, #2, #4, #5) remain on espressif32@7.0.1 — MCU #3 is the
only board that requires pioarduino.

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
| OLED SDA | GPIO16 | U8g2 HW_I2C (Wire bus 0); fall back to SW_I2C if pioarduino incompatible |
| OLED SCL | GPIO17 | same |
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

## FreeRTOS Task Architecture
| Task | Priority | Stack | Role |
|---|---|---|---|
| Receiver | 3 | STACK_SIZE_RECEIVER | Calls sharedBus.poll(), puts messages on inboundQueue |
| Logic | 2 | STACK_SIZE_LOGIC | Handles HEARTBEAT→ACK, routes DB_READ/DB_WRITE via queues |
| UART | 2 | STACK_SIZE_LOGIC | Owns Serial2 exclusively — sends requests to RPi, reads responses |
| OLED | 1 | STACK_SIZE_OLED | Reads SharedState under displayMutex every 500ms |

## Internal Queues
| Queue | Producer | Consumer | Depth | Purpose |
|---|---|---|---|---|
| inboundQueue | Receiver | Logic | 8 | Decoded bus messages (HEARTBEAT, DB_READ, DB_WRITE) |
| uartQueue | Logic | UART task | 4 | UART operation requests to RPi |
| uartResultQueue | UART task | Logic | 4 | UART operation results from RPi |

## Shared Bus Library
`shared_bus_wroom` — WROOM-specific implementation of the shared bus interface.
Same API as `shared_bus` (used by ESP32-C3 MCUs): init(), send(), poll().
Internally uses i2c_new_slave_device() (IDF 5.2+) — interrupt-driven, ISR
signals a FreeRTOS semaphore which poll() blocks on.

Selected at compile time via `#ifdef MCU_BOARD_WROOM`.

## Logic Flow
```
Receiver calls sharedBus.poll() → blocks on semaphore → ISR fires on message → puts on inboundQueue

Logic wakes on inboundQueue:

  HEARTBEAT from MCU #1:
  → sharedBus.send(ACK)
  → update SharedState

  DB_READ from MCU #2:
  → put UartRequest{op=read, account} on uartQueue
  → block on uartResultQueue (500ms timeout)
  → on timeout:  sharedBus.send(DB_READ_RESULT, Status::TIMEOUT)
  → on result:   sharedBus.send(DB_READ_RESULT, balance)
  → update SharedState

  DB_WRITE from MCU #2:
  → put UartRequest{op=write, account, newBalance, txnType, mid} on uartQueue
  → block on uartResultQueue (500ms timeout)
  → on timeout:  sharedBus.send(DB_WRITE_ACK, Status::TIMEOUT)
  → on result:   sharedBus.send(DB_WRITE_ACK, Status::OK)
  → update SharedState

UART task wakes on uartQueue:
  → serialize request to JSON
  → Serial2.println(json)
  → read until '\n' or 500ms timeout
  → put UartResult on uartResultQueue

OLED task wakes every 500ms:
  → take displayMutex
  → read SharedState
  → render to display
  → give displayMutex
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

## Critical Notes
- Do NOT call vTaskStartScheduler() — Arduino/pioarduino already started FreeRTOS
- Call vTaskDelete(NULL) in loop() to reclaim loopTask stack
- sharedBus.init(I2C_ADDRESS) must be called before xTaskCreate()
- Serial2 is UART1 — do not use Serial (UART0/USB) for RPi communication
- RPi must be booted before MCU #3 starts — UART timeout handles RPi-down gracefully
- Disconnect shared bus wires before flashing — bus activity during flash corrupts firmware
- GPIO8/9 availability must be verified on any future board replacement —
  these are crystal pins on some boards (LOLIN32 Lite) and unavailable
- pioarduino is MCU #3 only — do not change other MCUs' platform
- driver/i2c.h (old driver) must not be included in MCU #3 build once
  driver/i2c_slave.h is in use — conflict causes build failure
- If U8g2 HW_I2C hangs on begin() under pioarduino, use SW_I2C with GPIO log suppression