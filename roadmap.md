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

**MCU #3 + RPi DB backend** *(ADR-009 — SD card approach superseded)*
> SD card storage abandoned after three board replacements and exhaustive
> SPI debugging. MCU #3 remains on shared bus at 0x0A; storage delegated
> to Raspberry Pi 3B+ over UART. See ADR-009 and raspi-db-server/CLAUDE.md.
>
> MCU #3 Arduino I2C slave initialization caused TG1WDT on all attempted
> paths (Wire.begin() slave mode, i2c_driver_install() slave mode). Further,
> arduino-esp32 3.x auto-initializes Wire on GPIO8/9 before setup() runs,
> blocking i2c_new_slave_device() from claiming those pins even under
> pioarduino. Solution: switch to framework = espidf (pure ESP-IDF, no
> Arduino layer), which eliminates all framework-level GPIO conflicts at root.
> See ADR-010 for full investigation and decision chain.

- [x] MCU #3: ESP32-WROOM-32 DevKit confirmed on /dev/ttyUSB0, OLED working
- [x] RPi: OS installed, hardware UART freed from Bluetooth, SSH confirmed
- [x] RPi: UART echo server confirmed on ttyAMA0 (GPIO14/15)
- [x] MCU #3: UART loopback confirmed (GPIO18→GPIO19)
- [x] MCU #3 + RPi: UART echo round-trip confirmed (Arduino era)

**MCU #3 — Phase 0: ESP-IDF Project Skeleton**
> Framework: espressif32 @ 6.10.0 (IDF 5.4.0), framework = espidf.
> No Arduino layer. app_main() replaces setup()/loop().
> ESP_LOGI replaces Serial. vTaskDelay replaces delay().
> monitor_speed = 115200 (IDF console default — 921600 was Arduino-specific).

- [x] 0.1 Bare espidf project — boots, logs IDF version + I2C address, no WDT
- [x] 0.2 sdkconfig.defaults confirmed — CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2=y
      compiles and boots cleanly

**MCU #3 — Phase 1: UART to RPi**
> uart_driver_install() on UART1, GPIO18/19, 115200 baud.
> Handshake: RPi sends PING, MCU responds PONG (one-time at boot).
> Echo test confirms bidirectional data flow before moving to JSON protocol.

- [x] 1.1 UART1 init + handshake — RPi sends PING, MCU responds PONG, logged
- [x] 1.2 Echo test — MCU sends "hello from mcu3", RPi echoes back, PASS logged
- [~] 1.3 Timeout path — superseded by Phase 5.5 (UART task timeout, 500ms
      production path). uart_read_line() timeout confirmed working in handshake
      context (RPi-down loop observed in serial log).

**MCU #3 — Phase 2: OLED**
> U8g2 in HAL-callback mode via u8g2-hal-esp-idf community HAL.
> I2C_NUM_1 hardware master on GPIO16/17, legacy i2c driver (driver/i2c.h).
> HAL hardcodes 50kHz bus speed — acceptable for 500ms-refresh display.
> oled_display_wroom written as C++ wrapper around u8g2 C API + hal callbacks.
> extern "C" required around u8g2_esp32_hal.h — hal is C, not C++.

- [x] 2.1 U8g2 HAL callbacks — i2c_byte_cb and gpio_delay_cb wired via
      u8g2-hal-esp-idf. OLED shows static "DATABASE CTRL / Addr: 0x0A /
      Phase 2.1 / BOOT OK", persists across resets.
- [ ] 2.2 OLED task at priority 1, 500ms refresh, static content
      DoD: 60-second soak, no crash, OLED continuously updating

**MCU #3 — Phase 3: Shared Bus I2C Slave**
> Core objective. i2c_new_slave_device() on I2C_NUM_0 / GPIO8/9.
> No Arduino Wire anywhere — GPIO matrix conflict eliminated at root.
> shared_bus_wroom written from scratch — no Arduino dependencies.

- [ ] 3.1 i2c_new_slave_device() init — slave at 0x0A on GPIO8/9
      DoD: no "GPIO 8/9 is not usable" warning, no WDT, slave init OK in log
- [ ] 3.2 Receive via ISR callback — raw bytes from MCU #1 logged
      DoD: MCU #1 sends HEARTBEAT to 0x0A, MCU #3 logs bytes received
- [ ] 3.3 Send response — i2c_slave_write() ACK confirmed by MCU #1
      DoD: MCU #1 log shows ACK received from 0x0A
- [ ] 3.4 Wrap into shared_bus_wroom library — init()/send()/poll() interface
      DoD: same API as shared_bus (ESP32-C3 version)

**MCU #3 — Phase 4: FreeRTOS Task Architecture**
- [ ] 4.1 Receiver + Logic tasks — HEARTBEAT ACK on 5-MCU bus;
      DB_READ/DB_WRITE stubbed (log only)
      DoD: MCU #1 OLED shows MCU #3 alive in heartbeat health display
- [ ] 4.2 OLED task — SharedState updates every 500ms under displayMutex
      DoD: live read/write counters visible on OLED during bus activity

**MCU #3 — Phase 5: Full Integration**
- [ ] 5.1 UART task skeleton — uart_driver_install in task context, loopback confirmed
- [ ] 5.2 Logic → UART queue wiring — uartQueue/uartResultQueue, 500ms timeout fires
- [ ] 5.3 DB_READ end-to-end — MCU #2 receives correct balance from SQLite via RPi
- [ ] 5.4 DB_WRITE end-to-end — balance updated in SQLite, DB_WRITE_ACK to MCU #2
- [ ] 5.5 Timeout and fault handling — RPi-down returns Status::TIMEOUT within 500ms

**MCU #3 — Phase 6: Hardening**
- [ ] uxTaskGetStackHighWaterMark() logged at startup for all tasks — tune stack sizes
- [ ] WDT audit: all tasks block on queues or call vTaskDelay, never spin
- [ ] 10-minute soak with RPi cycling on/off — no WDT resets, no queue overflow

**RPi DB backend**
- [ ] RPi: db_server.py — DB_READ handler (SQLite accounts table, balance query)
- [ ] RPi: db_server.py — DB_WRITE handler (write-ahead log, balance update, commit)
- [ ] RPi: web_server.py — Flask transaction history table at :5000
- [ ] RPi: rsync cron job — SQLite backup to dev PC every 10 minutes

**Remaining MCUs**
- [ ] MCU #2: sequential transaction handling — one transaction at a time (Option A)
- [ ] MCU #4: immediate job dispatch — receive JOB_SUBMIT, dispatch to MCU #2 immediately
- [ ] MCU #5: WiFi/HTTP server task, web console (single pending request slot), I2C logic task
- [ ] MCU #1: serial console commands — DEPOSIT/WITHDRAW/BALANCE dispatch to MCU #4
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

---

## Phase 5 — Advanced Features

- [ ] Atomic TRANSFER transactions (two-phase commit across MCU #2 and MCU #3)
- [ ] Crash recovery — MCU #3 RPi replays PENDING transactions from SQLite log on boot
- [ ] Storage redundancy — periodic backup to USB stick on RPi
- [ ] Load testing — 50 sequential transactions without dropping
- [ ] Failure simulation — physically disconnect a subsystem, observe timeout propagation
- [ ] Priority job scheduling stress test — mixed HIGH/MEDIUM/LOW queue under load
- [ ] Binary message protocol — replace JSON with compact binary format
- [ ] USB-to-I2C PC dispatcher *(in consideration)*
- [ ] MCU #1 serial console account management — create/delete accounts at runtime
- [ ] Account balance limits and overdraft rules