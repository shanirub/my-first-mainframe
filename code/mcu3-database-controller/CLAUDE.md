Here's the updated MCU #3 CLAUDE.md:

```markdown
# MCU #3 — Database Controller

## Identity
- I2C address on shared bus: 0x0A
- Role: account storage — SD card read/write, balance queries, transaction log
- Board: ESP32-WROOM-32 DevKit (38-pin, CP2102, Type-C) — third board for this
  slot, see ADR-008
- upload_port = /dev/ttyUSB0
- monitor_port = /dev/ttyUSB0

## Hardware Note
MCU #3 has gone through two board replacements:

1. **ESP32-C3 SuperMini** — insufficient free GPIOs for shared bus + OLED +
   SPI SD card simultaneously. GPIO4–7 JTAG reserved, GPIO6/7 flash SPI pins,
   GPIO0/2 strapping pins. See ADR-008.

2. **WeMos LOLIN32 Lite clone** — GPIO8 and GPIO9 are connected to the 32kHz
   crystal oscillator (Xtal32N/Xtal32P) on this board and are not available
   as user GPIO. The shared bus requires GPIO8=SDA and GPIO9=SCL — hard
   architectural constraint. See ADR-008 amendment.

3. **ESP32-WROOM-32 DevKit (38-pin, CP2102)** — GPIO8/9 confirmed available
   as user GPIO per official Espressif pinout. VSPI pins (18/19/23/5) clean
   for SD card. All pin assignments unchanged from previous board.

## Pin Configuration
| Function | GPIO | Notes |
|---|---|---|
| Shared bus SDA | GPIO8 | Matches all other MCUs — hub wiring unchanged |
| Shared bus SCL | GPIO9 | Matches all other MCUs — hub wiring unchanged |
| OLED SDA | GPIO16 | U8g2 SW I2C — free, no special functions |
| OLED SCL | GPIO17 | U8g2 SW I2C — free, no special functions |
| SD MOSI | GPIO23 | ESP32 default VSPI MOSI |
| SD MISO | GPIO19 | ESP32 default VSPI MISO |
| SD SCK  | GPIO18 | ESP32 default VSPI SCK |
| SD CS   | GPIO5  | ESP32 default VSPI CS |

## SD Card
- Library: SdFat 2.3.1 (greiman/SdFat@^2.2.2) — SD.h failed with SDXC card
- Format: FAT32, partitioned (sda1, 8GB on 64GB physical card)
- Card currently formatted and ready — init not yet confirmed on new board
- Files (planned): accounts.json (current balances), transactions.log (append-only)
- Write-ahead log pattern: log entry written before balance update

## Current State (Phase 3 — awaiting replacement board)
- ESP32-WROOM-32 DevKit 38-pin ordered — replacing WeMos LOLIN32 Lite
- ESP32 Terminal Adapter (screw terminals) ordered — eliminates breadboard
  contact failures that caused intermittent SD errors
- Second SD card module ordered — for Phase 5 RAID-1
- SD card wired and formatted (FAT32 partitioned, sda1, 8GB)
- main.cpp, config.h, platformio.ini all ready — no firmware changes needed
  on board arrival
- FreeRTOS skeleton (heartbeat ACK) not yet tested on any replacement board

## Phase 3 Implementation Steps
1. [ ] Solder headers on new board, wire via terminal adapter
2. [ ] Flash SD init test — confirm card initializes, read/write test.txt
3. [ ] Migrate FreeRTOS skeleton (receiver/logic/OLED tasks), verify heartbeat ACK
4. [ ] 5-MCU bus test — confirm MCU #3 on shared bus with all others
5. [ ] Implement SD task (sdQueue + sdResultQueue pattern)
6. [ ] Implement DB_READ handler — read accounts.json, return DB_READ_RESULT
7. [ ] Implement DB_WRITE handler — write-ahead log, update accounts.json,
       return DB_WRITE_ACK
8. [ ] End-to-end test with MCU #2

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
- Dual SD card RAID-1 mirroring (Phase 5 — second SD module ordered)

## Critical Notes
- Do NOT call vTaskStartScheduler() — ESP32 Arduino already started FreeRTOS
- Call vTaskDelete(NULL) in loop() to reclaim loopTask stack
- sharedBus.init(I2C_ADDRESS) must be called before xTaskCreate()
- sdTest() must run before sharedBus.init() — bus init hangs on unconnected
  bus and triggers watchdog
- SdFat requires SPI.begin(SCK, MISO, MOSI, -1) before sd.begin()
- CS must be driven HIGH manually before sd.begin() — SD cards require CS
  idle high
- SdFat file paths do NOT use leading slash: "accounts.json" not "/accounts.json"
  (opposite of SD.h convention)
- SPI_DRIVER_SELECT=1 and SDFAT_FILE_TYPE=3 required in platformio.ini
  build_flags for correct ESP32 SdFat operation
- GPIO8/9 availability must be verified on any future board replacement —
  these are crystal pins on some boards (LOLIN32 Lite) and unavailable
```