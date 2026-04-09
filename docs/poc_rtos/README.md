# Phase 2.5 PoC — FreeRTOS Multi-Master I2C

## What this is

A minimal proof of concept validating the three core assumptions that underpin
the FreeRTOS architecture pivot documented in ADR-007. The PoC uses MCU #1 and
MCU #2 only. No other MCUs are involved.

## Why it exists

Phase 2 testing revealed that TwoWire on ESP32-C3 locks into master or slave
mode at boot. A slave cannot initiate transmissions. This prevents subsystem-to-
subsystem communication (e.g. MCU #4 → MCU #3) without routing everything
through MCU #1 — which distorts the mainframe architecture.

FreeRTOS tasks with a mutex-protected bus allow any MCU to temporarily become
master, transmit, then return to slave. Before redesigning all five MCUs around
this approach, we validate that it actually works on real hardware.

## Documents in this folder

| File | Purpose |
|---|---|
| `README.md` | This file — entry point and overview |
| `poc_plan.md` | Assumptions, task design, pass/fail criteria, failure paths |
| `sequence_diagrams.md` | Annotated sequence diagrams for boot and send/receive flow |
| `results.md` | Test results filled in after running on hardware |

## Relation to ADR-007

This PoC implements the validation plan defined at the bottom of ADR-007.
All three assumptions must pass before Phase 3 begins. If any assumption
fails, the failure path is documented in `poc_plan.md`.

## Scope

The PoC deliberately excludes:
- MCUs #3, #4, #5 — added in Phase 3 after PoC passes
- Subsystem business logic — heartbeat is used as a minimal message type
- SD card, WiFi, HTTP server — irrelevant to the transport assumptions

## What stays unchanged

OledDisplay, MessageProtocol, shared_config.h, and physical wiring are all
unchanged. Only SharedBus and main.cpp are modified for this PoC.
