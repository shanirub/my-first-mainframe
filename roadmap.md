# 🗺️ Project Roadmap

---

## Phase 1 — Foundation *(Week 1)*

- [x] Development environment setup (VS Code + PlatformIO + ESP32 board support)
- [x] Flash and verify all MCUs individually
- [x] Test OLED displays per subsystem (MCU1 and MCU2 confirmed working)
- [x] Shared OLED library created and working across all MCUs
- [x] PlatformIO workspace configured for all MCUs simultaneously
- [x] Establish basic I2C communication between two MCUs

---

## Phase 1.5 — Hardware Fix *(unplanned, before Phase 2)*

- [x] Replace TwoWire(1) OLED bus with U8g2 SW_I2C (ESP32-C3 has one I2C peripheral)
- [x] Verify OLED + shared bus running simultaneously on MCU #1 and MCU #2
- [x] Extract SharedBus class into shared library with poll() pattern
- [x] Extract MessageProtocol class into shared library with schema validation
- [x] Update all MCU main.cpp files to use config.h pin constants

---

## Phase 2 — Protocol *(Week 2)*

- [x] Implement JSON message format using ArduinoJson + MessageProtocol library
- [x] Connect all 5 MCUs to shared I2C bus (T-shape layout, hub on vertical BB)
- [x] Test MCU #1 ↔ each slave individually (sequential pair tests) ✅

> Note: full 5-MCU simultaneous bus test deferred — see Phase 2.5 below.

---

## Phase 2.5 — Architecture Pivot: FreeRTOS *(complete 2026-04-13)*

The Arduino loop() model was found insufficient for authentic mainframe
subsystem-to-subsystem communication. Key constraint: TwoWire on ESP32-C3
can only be master OR slave per boot — slaves cannot initiate transmissions.
This prevents JES (MCU #4) from talking directly to DASD (MCU #3) without
routing through MCU #1, which distorts the mainframe architecture.

FreeRTOS — already running under Arduino on ESP32 — surfaces as the right
abstraction layer. Tasks map naturally to subsystems, queues model job
scheduling, mutexes protect bus access.

**See ADR-007 for full reasoning and validation results.**

- [x] Identify I2C master/slave constraint via testing
- [x] Document pivot reasoning (ADR-007)
- [x] Validate FreeRTOS multi-master I2C on ESP32-C3 (proof of concept)
- [x] Redesign SharedBus for task-safe operation (mutex + mode switching)
- [x] Validate all three PoC assumptions on real hardware (MCU #1 + MCU #2)
- [x] Design full 5-MCU FreeRTOS task architecture (docs/design/freertos_architecture.md)

---

## Phase 3 — Individual Subsystems *(simple implementations)*

> FreeRTOS task pattern used throughout. Each MCU implemented and tested
> incrementally: receiver + OLED first, then logic, then subsystem task,
> then full flow. See docs/design/freertos_architecture.md for task design.
> Implementation order follows dependency chain: DB first, then Transaction
> Processor, then Job Scheduler, then I/O Controller, then serial console.

- [x] Migrate MCUs #3, #4, #5 to FreeRTOS task pattern (init() API)
- [x] Full 5-MCU simultaneous bus test
- [x] MCU #1: heartbeat task (single sender, all 4 slaves, timestamp-based health tracking)

---

### RPi — Database Controller (address 0x0A)

> ESP32-WROOM-32 DevKit (MCU #3) retired after exhaustive I2C slave
> debugging across three boards and three frameworks. Raspberry Pi 3B+
> replaces it directly on the shared bus at 0x0A via the BCM2837 BSC slave
> peripheral (pigpio bsc_i2c, GPIO18/19). See ADR-011 for full history.
>
> Phase 3 covers bus participation and OLED only. SQLite and Flask are
> Phase 4. Updated per ADR-011 (2026-06-03).

**RPi — Phase 3.1: OLED smoke test**
- [ ] SSD1306 on GPIO2/3 (BSC1 master), static text on screen
      DoD: "DATABASE CTRL / Addr: 0x0A / BOOT OK" persists across resets

**RPi — Phase 3.2: pigpio BSC slave init**
- [ ] pigpio daemon running, `bsc_i2c(0x0A)` armed on GPIO18/19
      DoD: loopback test — RPi sends to itself, callback fires, logged

**RPi — Phase 3.3: HEARTBEAT receive**
- [ ] MCU #1 sends HEARTBEAT broadcast, RPi callback fires and logs it
      DoD: RPi serial/SSH log shows HEARTBEAT received from 0x08, no response sent yet

**RPi — Phase 3.4: HEARTBEAT_ACK round-trip**
- [ ] RPi loads HEARTBEAT_ACK into TX FIFO with ready_byte = 0x01
- [ ] MCU #1 calls sendAndReceive(0x0A), reads ACK, logs success
      DoD: MCU #1 OLED shows RPi alive in heartbeat health display

**RPi — Phase 3.5: sendAndReceive() on MCU #2**
- [ ] sendAndReceive() added to SharedBus — holds busMutex across send + poll loop
- [ ] MCU #2 sends DB_READ stub, polls until ready_byte = 0x01, reads 256 bytes
- [ ] RPi loads stub DB_READ_RESULT (hardcoded balance, no SQLite yet)
      DoD: MCU #2 logs correct stub balance received from 0x0A

**RPi — Phase 3.6: DB_WRITE_ACK stub round-trip**
- [ ] MCU #2 sends DB_WRITE stub, RPi loads stub DB_WRITE_ACK
      DoD: MCU #2 receives ACK within 500ms, logs success

**RPi — Phase 3.7: OLED live updates**
- [ ] oled_worker thread running, display_state updated on each bus event
- [ ] OLED shows: IDLE / DB READ / DB WRITE + last account number
      DoD: OLED updates correctly during a stub DB_READ + DB_WRITE cycle

**RPi — Phase 3.8: timeout path**
- [ ] RPi process killed mid-transaction, MCU #2 poll loop expires
      DoD: MCU #2 returns Status::TIMEOUT within 500ms, logged correctly

---

### Remaining MCUs — Phase 3

- [ ] MCU #2: sequential transaction handling — one transaction at a time (Option A)
      Requires sendAndReceive() from Phase 3.5 above
- [ ] MCU #4: immediate job dispatch — receive JOB_SUBMIT, dispatch to MCU #2 immediately
- [ ] MCU #5: WiFi/HTTP server task, web console (single pending request slot), I2C logic task
- [ ] MCU #1: serial console commands — DEPOSIT/WITHDRAW/BALANCE dispatch to MCU #4
      Requires sendAndReceive() for HEARTBEAT_ACK from Phase 3.4 above
- [ ] End-to-end transaction flow verified: DEPOSIT, WITHDRAW, BALANCE

---

## Phase 4 — Integration and Production Implementations

- [ ] MCU #2: replace sequential logic with state machine (Option B)
- [ ] MCU #4: replace immediate dispatch with priority queue — HIGH/MEDIUM/LOW ordering
- [ ] MCU #5: expand pending request table to 4 slots (from 1)
- [x] Heartbeat and health monitoring — MCU #1 flags non-responding subsystems on OLED
- [ ] Single retry on BusError::NOT_FOUND before reporting failure
- [ ] Full banking scenarios verified on real hardware
- [ ] MCU #1: WiFi web dashboard
- [ ] RTC timestamps on all transaction log entries (DS1307 on MCU #1)
- [ ] RPi: db.py — DB_READ handler (SQLite accounts table, balance query)
- [ ] RPi: db.py — DB_WRITE handler (write-ahead log pattern, balance update, COMMITTED)
- [ ] RPi: web_server.py — Flask transaction history table at :5000
- [ ] RPi: rsync cron job — SQLite backup to dev PC every 10 minutes

---

## Phase 5 — Advanced Features

- [ ] Atomic TRANSFER transactions (two-phase commit across MCU #2 and RPi)
- [ ] Crash recovery — RPi replays PENDING transactions from SQLite log on boot
- [ ] Storage redundancy — periodic backup to USB stick on RPi
- [ ] Load testing — 50 sequential transactions without dropping
- [ ] Failure simulation — physically disconnect a subsystem, observe timeout propagation
- [ ] Priority job scheduling stress test — mixed HIGH/MEDIUM/LOW queue under load
- [ ] Binary message protocol — replace JSON with compact binary format
- [ ] USB-to-I2C PC dispatcher *(in consideration)*
- [ ] MCU #1 serial console account management — create/delete accounts at runtime
- [ ] Account balance limits and overdraft rules