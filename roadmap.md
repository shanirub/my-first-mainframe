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

- [x] Migrate MCUs #3, #4, #5 to FreeRTOS task pattern (init() API)
- [x] Full 5-MCU simultaneous bus test
- [x] MCU #1: heartbeat task (single sender, all 4 slaves, timestamp-based health tracking)
- [ ] MCU #1: serial console commands — DEPOSIT/WITHDRAW/BALANCE dispatch to MCU #4
- [ ] MCU #2: sequential transaction handling — one transaction at a time (Option A)
- [ ] MCU #3: SD card read/write via dedicated SD task (accounts.json + transactions.log)
- [ ] MCU #4: immediate job dispatch — receive JOB_SUBMIT, dispatch to MCU #2 immediately
- [ ] MCU #5: WiFi/HTTP server task, web console (single pending request slot), I2C logic task
- [ ] End-to-end transaction flow verified: DEPOSIT, WITHDRAW, BALANCE

---

## Phase 4 — Integration and Production Implementations

> Replace Phase 3 simple implementations with production-grade designs.
> The surrounding task/queue structure is unchanged — only logic internals
> are replaced. All Phase 3 code is scaffold, not throwaway.

- [ ] MCU #2: replace sequential logic with state machine (Option B) — enables concurrent transactions
- [ ] MCU #4: replace immediate dispatch with priority queue — HIGH/MEDIUM/LOW ordering
- [ ] MCU #5: expand pending request table to 4 slots (from 1)
- [x] Heartbeat and health monitoring — MCU #1 flags non-responding subsystems on OLED
- [ ] Single retry on BusError::NOT_FOUND before reporting failure
- [ ] Full banking scenarios verified on real hardware (deposit, withdrawal, balance, insufficient funds)
- [ ] MCU #1: WiFi web dashboard (replaces serial monitor for operator commands)
- [ ] RTC timestamps on all transaction log entries (DS1307 on MCU #1)

---

## Phase 5 — Advanced Features

- [ ] Atomic TRANSFER transactions (two-phase commit across MCU #2 and MCU #3)
- [ ] Crash recovery — MCU #3 replays incomplete transactions from write-ahead log on boot
- [ ] Load testing — 50 sequential transactions without dropping
- [ ] Failure simulation — physically disconnect a subsystem, observe timeout and error propagation
- [ ] Priority job scheduling stress test — mixed HIGH/MEDIUM/LOW queue under load
- [ ] Binary message protocol — replace JSON with compact binary format for performance comparison
- [ ] USB-to-I2C PC dispatcher *(in consideration)* — connect PC directly to shared bus via CH341/MCP2221 adapter for automated test injection and scripted transaction sequences
- [ ] MCU #1 serial console account management — create/delete accounts at runtime
- [ ] Account balance limits and overdraft rules