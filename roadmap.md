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

## Phase 2.5 — Architecture Pivot: FreeRTOS *(replanning in progress)*

The Arduino loop() model was found insufficient for authentic mainframe
subsystem-to-subsystem communication. Key constraint: TwoWire on ESP32-C3
can only be master OR slave per boot — slaves cannot initiate transmissions.
This prevents JES (MCU #4) from talking directly to DASD (MCU #3) without
routing through MCU #1, which distorts the mainframe architecture.

FreeRTOS — already running under Arduino on ESP32 — surfaces as the right
abstraction layer. Tasks map naturally to subsystems, queues model job
scheduling, mutexes protect bus access.

**See ADR-007 for full reasoning.**

- [x] Identify I2C master/slave constraint via testing
- [x] Document pivot reasoning (ADR-007)
- [ ] Validate FreeRTOS multi-master I2C on ESP32-C3 (proof of concept)
- [ ] Redesign SharedBus for task-safe operation (mutex + mode switching)
- [ ] Redesign each MCU's main.cpp as FreeRTOS tasks
- [ ] Full 5-MCU simultaneous bus test with FreeRTOS architecture

---

## Phase 3 — Individual Subsystems *(Weeks 3–4)*

> To be replanned after FreeRTOS validation. Core subsystem roles unchanged.

- [ ] MCU #1: heartbeat task, serial console input, logging task
- [ ] MCU #2: transaction validation and routing tasks
- [ ] MCU #3: SD card read/write tasks (accounts.json + transactions.log)
- [ ] MCU #4: priority job queue task, dispatcher task
- [ ] MCU #5: WiFi/HTTP task, web console, I/O handler task

---

## Phase 4 — Integration *(Week 5)*

- [ ] End-to-end transaction flow (deposit, withdrawal, balance)
- [ ] Heartbeat and health monitoring across all subsystems
- [ ] Full banking scenarios verified on real hardware

---

## Phase 5 — Advanced Features *(Week 6)*

- [ ] Priority job scheduling with HIGH / MEDIUM / LOW queues
- [ ] Atomic transfer transactions (two-phase commit)
- [ ] Load testing (50 sequential transactions without dropping)
- [ ] Failure simulation (physical subsystem disconnection)
- [ ] Crash recovery via write-ahead log on MCU #3
