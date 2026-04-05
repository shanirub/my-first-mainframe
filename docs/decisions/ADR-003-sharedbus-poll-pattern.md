# ADR-003: SharedBus class with poll() pattern

## Status
Accepted

## Context
Initial implementation had raw TwoWire calls directly in each MCU's main.cpp.
This created several problems:

1. onReceive() callback runs in ISR (interrupt) context. Any I2C call inside
   the callback (e.g. updating OLED) causes deadlock or memory corruption.
2. TwoWire.available() without read() is an incomplete API — data cannot
   be consumed.
3. Bus initialisation code would be duplicated across all 5 MCUs.
4. Arduino's onReceive() requires a plain C function pointer — member
   functions cannot be registered directly.

## Decision
Encapsulate all shared bus logic in a SharedBus class in shared/libs/shared_bus/.
Public interface:

    SharedBus();
    void beginMaster();
    void beginSlave(uint8_t address);
    BusError send(uint8_t targetAddress, const char* message);
    bool poll(char* buf, int bufLen);

## Reasoning
- ISR fills internal ring buffer only — no application code runs in interrupt
  context. poll() drains buffer safely from loop().
- poll() called from loop() means OLED updates, serial logging, and any other
  operations are safe alongside bus reception.
- BusError return type preserves endTransmission() error codes with named
  constants (OK, NOT_FOUND, BUS_FAULT, TIMEOUT) — more useful than raw int.
- Static _instance pointer solves the C function pointer constraint:
  staticISR (plain C, no this) uses _instance to reach the live object.
  One SharedBus per MCU is an enforced constraint.
- Abstraction means main.cpp never touches TwoWire directly — bus
  implementation can change without touching application code.

## Consequences
- One SharedBus instance per MCU — enforced by static _instance pattern
- loop() must call sharedBus.poll() on every iteration — no delay() in loop
- MCU slave loop() can no longer just delay(100) — must actively poll
- This is the correct pattern for Phase 2: every MCU's loop checks bus,
  updates display, checks serial, repeats
