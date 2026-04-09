# PoC Results

**Date tested:** _____________
**Hardware:** MCU #1 (0x08) + MCU #2 (0x09) on shared bus
**Firmware commit:** _____________

---

## A1 — TwoWire mode switching

**Result:** PASS / FAIL

**Cycles run:** ___ / 20 minimum

**BUS_FAULT errors observed:** ___

**ACKs received on MCU #1:** ___

**Notes:**

```
paste relevant serial output here
```

---

## A2 — FreeRTOS queue integrity

**Result:** PASS / FAIL

**Total sends (MCU #1):** ___
**Total receives (MCU #2):** ___
**Messages dropped:** ___
**QUEUE_FULL errors:** ___

**Notes:**

```
paste relevant serial output here
```

---

## A3 — Mutex protection

**Result:** PASS / FAIL

**Validation failures on MCU #2:** ___
**PulseView overlapping transactions observed:** yes / no

**Notes:**

```
paste relevant serial output here
```

---

## Overall result

**PASS — proceed to Phase 3** / **FAIL — see failure path in poc_plan.md**

**Unexpected findings:**

**Changes made during testing (if any):**

---

## Next step

If all pass: update ADR-007 status, begin SharedBus v2 design for all 5 MCUs,
update roadmap Phase 2.5 checklist.

If any fail: document which assumption failed, which failure path from
poc_plan.md applies, and what the revised approach is before retesting.
