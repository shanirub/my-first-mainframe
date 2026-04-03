# 🗺️ Project Roadmap

---

## Phase 1 — Foundation *(Week 1)*

- [x] Development environment setup (VS Code + PlatformIO + ESP32 board support)
- [x] Flash and verify all MCUs individually
- [x] Test OLED displays per subsystem (MCU1 and MCU2 confirmed working)
- [x] Shared OLED library created and working across all MCUs
- [x] PlatformIO workspace configured for all MCUs simultaneously
- [X] Establish basic I2C communication between two MCUs

---

## Phase 1.5 — Hardware Fix (unplanned, before Phase 2)

- [x] Replace TwoWire(1) OLED bus with SoftWire (ESP32-C3 has one I2C peripheral)
- [x] Verify OLED + shared bus running simultaneously on MCU #1
- [x] Verify OLED + shared bus running simultaneously on MCU #2
- [x] Extract SharedBus class from main.cpp into shared library
- [x] Update all MCU main.cpp files to use config.h pin constants (remove raw defines)

---

## Phase 2 — Protocol *(Week 2)*

- [ ] Implement JSON message format using ArduinoJson
- [ ] Connect all 5 MCUs to shared I2C bus
- [ ] Test multi-MCU message broadcasting and addressing

---

## Phase 3 — Individual Subsystems *(Weeks 3–4)*

- [ ] Build Master Console: serial input + WiFi web dashboard
- [ ] Build Database Controller: SD card read/write with JSON storage
- [ ] Build Transaction Processor: validation logic and routing
- [ ] Build Job Scheduler: priority queue implementation
- [ ] Build I/O Controller: transaction generation and result handling
- [ ] Test each subsystem independently before integration

---

## Phase 4 — Integration *(Week 5)*

- [ ] Connect all subsystems and run end-to-end transaction flow
- [ ] Implement and test full banking scenarios (deposit, withdrawal, transfer)
- [ ] Validate heartbeat and health monitoring across subsystems

---

## Phase 5 — Advanced Features *(Week 6)*

- [ ] Priority job scheduling with HIGH / MEDIUM / LOW queues
- [ ] Atomic transfer transactions (all-or-nothing semantics)
- [ ] Load testing (50 concurrent transactions)
- [ ] Failure simulation (physical subsystem disconnection)
- [ ] Logic analyzer debugging sessions on I2C bus traffic
