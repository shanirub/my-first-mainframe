# ADR-007: FreeRTOS tasks replace Arduino loop() for subsystem architecture

## Status
Accepted — supersedes single-loop approach from Phases 1–2

## Context
Phase 2 testing revealed a fundamental constraint: TwoWire on ESP32-C3
can only be in master OR slave mode per boot. Once `beginSlave()` is called,
the bus instance is permanently in slave mode and cannot initiate transmissions.

This means:
- MCU #4 (Job Scheduler / JES equivalent) cannot send directly to MCU #3
  (Database Controller / DASD equivalent) without routing through MCU #1
- Any slave-initiated response (HEARTBEAT_ACK, DB_READ_RESULT, JOB_COMPLETE)
  fails with "Bus is in Slave Mode" at the Wire driver level
- Confirmed in testing: MCU #2 correctly received HEARTBEAT but
  sendAck() failed — `[Wire.cpp:411] beginTransmission(): Bus is in Slave Mode`

Routing all communication through MCU #1 as a proxy was considered (Option A)
but rejected because:
- MCU #1 becomes a bottleneck and single point of failure
- It misrepresents mainframe architecture — JES communicates with DASD
  directly, not through the operator console
- It would require MCU #1 to understand the semantics of every message type

## Decision
Adopt FreeRTOS tasks for all MCUs starting Phase 2.5.

Each MCU runs a set of tasks rather than a single loop():
- Bus listener task (high priority) — ISR wakes task, task processes incoming messages
- Sender task (normal priority) — acquires bus mutex, switches to master, sends, releases
- Subsystem logic task (normal priority) — handles business logic per MCU role
- OLED task (low priority) — updates display independently

SharedBus is redesigned to be task-safe:
- Mutex protects all bus access (prevents simultaneous master attempts)
- beginMaster() / beginSlave() can be called at runtime to switch modes
- Any MCU can initiate communication when it holds the mutex

## Reasoning
- FreeRTOS is already running under Arduino on ESP32-C3 — this surfaces
  rather than adds it. No new dependency.
- Task-per-concern maps naturally to mainframe subsystem architecture.
  Each task IS a subsystem concern running concurrently.
- Mutex-protected bus access solves multi-master safely — I2C hardware
  arbitration handles simultaneous attempts at the electrical level.
- Queue-based message passing between tasks models job scheduling authentically.
- MCU #5's WiFi/HTTP server and I2C bus can run concurrently as separate tasks —
  this was going to be fragile in the loop() model.

## What Phase 1–2 produced that remains valid
The low-level work was not wasted — it built the foundation:
- Confirmed all 5 MCUs communicate correctly on the shared bus
- Validated SharedBus and MessageProtocol libraries (unchanged by this pivot)
- Understood exactly why the constraint exists (SOC_I2C_NUM=1, Wire mode exclusivity)
- Established the physical layout, wiring, and config architecture (all unchanged)
- Identified the constraint through proper testing rather than guessing

## What changes
- main.cpp structure: setup() creates tasks, loop() becomes minimal or empty
- SharedBus: add mutex, add runtime mode switching
- Communication pattern: any MCU can initiate — no single master required

## What stays the same
- All 5 MCU roles and I2C addresses
- OledDisplay library
- MessageProtocol library (field constants, validation, UUID generation)
- shared_config.h
- Physical wiring, hub, pull-ups

## Validation plan
Before rewriting all MCUs, validate three assumptions:
1. TwoWire mode switching (master→slave→master) works on ESP32-C3 at runtime
2. FreeRTOS queue correctly passes messages between tasks without data loss
3. Mutex-protected bus access prevents corruption under concurrent send attempts

All three must pass on a 2-MCU test before full migration.

## Consequences
- Phase 3 subsystem logic implementations will use FreeRTOS task pattern
- SharedBus v2 required before Phase 3 begins
- Each MCU will have a well-defined task list in its CLAUDE.md
