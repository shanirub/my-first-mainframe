# MCU #3 — Database Controller

## Identity
- I2C address on shared bus: 0x0A
- Role: account storage — SD card read/write, balance queries, transaction log
- Board: WeMos LOLIN32 Lite clone (ESP32-D0WDQ6, CH340C) — replaced ESP32-C3 SuperMini (see ADR-008)
- upload_port = /dev/ttyUSB0
- monitor_port = /dev/ttyUSB0

## Hardware Note
MCU #3 was migrated from ESP32-C3 SuperMini to WeMos LOLIN32 Lite because the
SuperMini has insufficient free GPIOs to run shared bus + OLED + SPI SD card
simultaneously. GPIO4–7 are JTAG reserved, GPIO6/7 are flash SPI pins,
GPIO0/2 are strapping pins — leaving too few clean GPIOs for all four
peripherals. See ADR-008 for full reasoning.

## Pin Configuration
| Function | GPIO | LOLIN32 Lite label | Notes |
|---|---|---|---|
| Shared bus SDA | GPIO8 | GPIO8 | Matches all other MCUs — hub wiring unchanged |
| Shared bus SCL | GPIO9 | GPIO9 | Matches all other MCUs — hub wiring unchanged |
| OLED SDA | GPIO16 | RXD2 | U8g2 SW I2C — free, no special functions |
| OLED SCL | GPIO17 | TXD2 | U8g2 SW I2C — free, no special functions |
| SD MOSI | GPIO23 | V_SPI_D / MOSI | ESP32 default VSPI MOSI |
| SD MISO | GPIO19 | V_SPI_Q / MISO | ESP32 default VSPI MISO |
| SD SCK  | GPIO18 | V_SPI_CLK / SCK | ESP32 default VSPI SCK |
| SD CS   | GPIO5  | V_SPI_CS0 / SS  | ESP32 default VSPI CS — onboard LED on this pin, will flicker during SPI (cosmetic only) |

## SD Card
- Library: Arduino SD.h (not SdFat — unstable on ESP32-C3, not tested needed on DevKit)
- Format: FAT32, 8GB partition on 64GB physical card
- Files: accounts.json (current balances), transactions.log (append-only)
- Write-ahead log pattern: log entry written before balance update

## Current State (Phase 3 — SD hardware test in progress)
- Board soldered, wired, recognized on /dev/ttyUSB0
- SD card wired: GPIO19=MISO, GPIO23=MOSI, GPIO18=SCK, GPIO5=CS
- SD card formatted FAT32 (8GB partition)
- Next step: flash SD init test, verify card initializes cleanly
- FreeRTOS skeleton (heartbeat ACK) not yet migrated to new board

## Phase 3 Implementation Steps
1. [ ] Flash SD init test — confirm card initializes, read/write test.txt
2. [ ] Migrate FreeRTOS skeleton to ESP32 DevKit (receiver/logic/OLED tasks)
3. [ ] Verify heartbeat ACK working on new board (5-MCU bus test)
4. [ ] Implement SD task (sdQueue + sdResultQueue pattern)
5. [ ] Implement DB_READ handler — read accounts.json, return DB_READ_RESULT
6. [ ] Implement DB_WRITE handler — write-ahead log, update accounts.json, return DB_WRITE_ACK
7. [ ] End-to-end test with MCU #2

## Phase 3 Tasks
| Task | Priority | Stack | Role |
|---|---|---|---|
| Receiver | 3 | STACK_SIZE_RECEIVER | Blocks on rxSemaphore, puts messages on inboundQueue |
| Logic | 2 | STACK_SIZE_LOGIC | Handles HEARTBEAT→ACK, DB_READ, DB_WRITE |
| SD | 2 | STACK_SIZE_LOGIC | Dedicated SD card task — owns all SD access |
| OLED | 1 | STACK_SIZE_OLED | Updates display every 500ms under displayMutex |

## Internal Queues
| Queue | Producer | Consumer | Depth | Purpose |
|---|---|---|---|---|
| inboundQueue | Receiver | Logic | 8 | incoming DB_READ and DB_WRITE requests |
| sdQueue | Logic | SD task | 4 | SD operation requests |
| sdResultQueue | SD task | Logic | 4 | SD operation results |

## Phase 3 Logic
```
receive DB_READ from MCU #2
→ put SdRequest on sdQueue (operation=SD_READ, account=...)
→ block on sdResultQueue waiting for result
→ send DB_READ_RESULT to MCU #2

receive DB_WRITE from MCU #2
→ put SdRequest on sdQueue (operation=SD_WRITE, account=..., newBalance=...)
→ SD task: append to transactions.log first (write-ahead)
→ SD task: update accounts.json
→ put result on sdResultQueue
→ send DB_WRITE_ACK to MCU #2
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

## Phase 4 Additions
- Crash recovery: on boot, check transactions.log for incomplete entries and replay
- Dual SD card RAID-1 mirroring (Phase 5 — second SD module already available)

## Critical Notes
- Do NOT call vTaskStartScheduler() — ESP32 Arduino already started FreeRTOS
- Call vTaskDelete(NULL) in loop() to reclaim loopTask stack
- sharedBus.init(I2C_ADDRESS) must be called before xTaskCreate()
- SD.h requires SPI.begin() before SD.begin() with explicit pin mapping
- SD.h file paths require leading slash: "/accounts.json" not "accounts.json"