# ADR-012: Retire the shared-bus subsystem architecture; begin mainframe-core redesign

## Status
Accepted — 2026-06-18
Terminal ADR for this project. The shared-bus architecture (ADR-008 through
ADR-011) is retired in full. Work continues in a successor project (see
"Successor" below).

## Supersedes
ADR-011 (RPi as I2C slave on the shared bus). Closes the ADR-008 → ADR-011 line.

---

## Context

The Database Controller subsystem was never reliably realized on the shared I2C
bus. The immediate trigger was a hardware constraint found at Phase 3.3: the
BCM2837 BSC (Broadcom Serial Controller) slave peripheral that `pigpio
bsc_i2c()` drives has a 16-byte hardware FIFO (RX and TX), which conflicts with
the 256-byte fixed-buffer TX protocol the design assumed.

Investigation of the `pigpio` source (`bscXfer()` in `pigpio.c`) established
that the 16-byte FIFO is *not* a hard message-size limit — pigpio drains it in a
software busy-loop into a 512-byte buffer while the bus transaction is active.
But the servicing is PIO (programmed I/O — the CPU hand-copies each byte) with
no DMA: a message larger than 16 bytes at 400 kHz is received intact only if the
daemon is already servicing the FIFO *during* the transaction, and the 16-byte
slack overruns in roughly 360 µs. Reliable reception is therefore a timing
gamble on a loaded Linux host, not a guarantee.

*Calibration: the size question is closed (pigpio accumulates, so a chunking
protocol is not strictly forced); the reliability question is not closed, and
was judged not worth resolving empirically given everything below.*

More important than the trigger is the pattern behind it. This was the **fourth
manifestation of one structural mismatch**: I2C used for two roles it is poorly
suited to —

1. multi-master peer messaging with runtime master↔slave mode-switching (the
   heartbeat/ACK mesh, documented as very difficult to stabilize), and
2. slave mode on devices with weak slave support — ESP32 I2C slave
   initialization (ADR-010: `TG1WDT_SYS_RESET` watchdog resets inside untested
   IDF driver paths) and the Linux/BSC slave (this ADR).

ADR-008 (SD card → three board swaps), ADR-009 (UART backend), ADR-010
(framework switch to pure ESP-IDF), and ADR-011 (RPi as BSC slave) were each
well-reasoned escapes from this same trap. Each moved the failure rather than
removing it. Continuing to patch the transport is continuing to fight the bus.

---

## Decision

**End this project.** The shared-bus-of-co-equal-subsystems premise is retired
in full — not patched, not re-transported. No further ADRs extend the
ADR-008–011 line.

The decision is deliberate and architectural, not a workaround. The recurring
failures are diagnosed as a wrong-bus-for-the-role mismatch; the correct
response is to change the architecture that forces those roles, not the wire
beneath it.

---

## Successor

A new project replaces this one. In it, the **Raspberry Pi becomes the mainframe
central complex**: the CICS-analogue, the transaction processor, and the job
scheduler run on the Pi as software subsystems, and durable storage (SQLite) is
local to the Pi. The four ESP32-C3 MCUs become **channel-attached peripherals**
(operator console, terminals, I/O channels) rather than co-equal bus subsystems.
The inter-device transport is reselected from a bus suited to the role.

The successor's design is intentionally **not** elaborated here. It will have its
own ADR-001 and design documents.

---

## What carries forward

The transport was the failure; the system above it was not. These are sound and
migrate to the successor as concepts:

- The subsystem decomposition and the CICS-minimal transaction flow
  (deposit / withdraw / balance → durable storage → acknowledge).
- The message semantics (HEARTBEAT/ACK, DB_READ/DB_WRITE, job
  submit/dispatch/complete).
- The FreeRTOS task pattern on the MCUs and the SQLite/WAL durability plan.
- All physical hardware: the 4 ESP32-C3 MCUs, the RPi 3B+, the 5 OLEDs, the
  power arrangement, the breadboards, and the logic analyzer.

---

## Consequences

- The shared I2C inter-device bus (hub, 5 kΩ pull-ups) and the Pi BSC slave
  (GPIO18/19) are retired. I2C remains in use only for the private OLED
  displays.
- The ESP32-WROOM-32 (already retired, ADR-008/011) stays retired. The
  `shared_bus` / `shared_bus_wroom` mode-switching libraries and the
  `bus_slave.py` BSC code are inactive — preserved in the repo as the
  investigation record, not deleted.
- This repo is frozen at this ADR. New work begins in the successor project.
- No functional requirement changes. The goal — a minimal CICS analogue with
  durable, reboot-surviving storage — is unchanged. Only the architecture that
  delivers it changes.
