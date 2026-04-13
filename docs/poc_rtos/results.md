# PoC Results

**Date tested:** 2026-04-13
**Hardware:** MCU #1 (0x08) + MCU #2 (0x09) on shared bus
**Firmware commit:** 2bda77e3c3be0150158649a5ccb1035f20ed522f

---

## A1 — TwoWire mode switching

**Result:** PASS

**Cycles run:** 200+

**BUS_FAULT errors observed:** 0

**ACKs received on MCU #1:** matches MCU #2 RX count exactly (see A2)

**Notes:**

Occasional `NOT_FOUND` (BusError value 1) errors observed on both Sender A
and Sender B. These are clean I2C NACKs — not bus corruption. They occur when
MCU #2 is briefly in master mode sending a HEARTBEAT_ACK and cannot receive
the next incoming heartbeat during that window. This is expected half-duplex
I2C behavior, not a mode switching failure.

No `BUS_FAULT` errors observed at any point. Mode switching via `_bus.end()`
→ master init → transmit → slave reinit is stable across hundreds of cycles.

---

## A2 — FreeRTOS queue integrity

**Result:** PASS

**Total sends (MCU #1):** A: ~137 + B: ~92 = ~229 at time of reading
**Total receives (MCU #2):** ~240 (slightly ahead due to fast increment)
**Messages dropped:** 0
**QUEUE_FULL errors:** 0

**Notes:**

MCU #2 RX count tracks MCU #1 A+B send count exactly when read at the same
moment. The apparent small difference in the snapshot above is due to the
counts changing rapidly during manual copying — confirmed in sync on a second
reading.

MCU #1 ACK count also matches MCU #2 RX count exactly, confirming that every
message received by MCU #2 produced a successful ACK back to MCU #1. No
messages were dropped at any queue stage.

---

## A3 — Mutex protection

**Result:** PASS

**Validation failures on MCU #2:** 0

**PulseView overlapping transactions observed:** pending photo capture

**Notes:**

Zero `MessageProtocol::validate()` failures observed on MCU #2 across the
entire test run. Errors on MCU #1 appear as clean `NOT_FOUND` NACKs
alternating between Sender A and Sender B — never simultaneous, confirming
the mutex is correctly serializing bus access. No garbled JSON was produced.

---

## Overall result

**PASS — proceed to Phase 3**

**Unexpected findings:**

`vTaskStartScheduler()` must not be called explicitly on ESP32 Arduino — the
framework already starts FreeRTOS before `setup()` runs. Calling it a second
time crashes with `ESP_ERR_NOT_FOUND` on the systick interrupt allocation.
Fix: remove `vTaskStartScheduler()` from `setup()`. Tasks created with
`xTaskCreate()` start running immediately since the scheduler is already
active. Added `vTaskDelete(NULL)` in `loop()` to reclaim loopTask stack.

**Changes made during testing:**

- Removed `vTaskStartScheduler()` from both MCU #1 and MCU #2 `setup()`
- Added `vTaskDelete(NULL)` to `loop()` in both MCUs

---

## Captures

`docs/captures/poc_rtos_result.jpg` — MCU #1 OLED showing send counts and ACK count. MCU #2 OLED showing RX count and last message type.

---

## Next step

All three assumptions pass. Proceed to:

1. Update ADR-007 status to reflect PoC completion
2. Update roadmap — check off Phase 2.5 PoC items
3. Design full 5-MCU FreeRTOS task architecture
4. Redesign SharedBus for all five MCUs
5. Migrate MCUs #3, #4, #5 main.cpp to FreeRTOS task pattern