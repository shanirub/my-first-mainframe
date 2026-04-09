# PoC Plan — FreeRTOS Multi-Master I2C

## Assumptions under test

Three assumptions must all pass before the full FreeRTOS migration proceeds.

---

### A1 — TwoWire mode switching works at runtime

**What we are testing:** Can a single TwoWire(0) instance call `_bus.end()`
followed by `_bus.begin()` repeatedly at runtime to switch between master and
slave mode on ESP32-C3 Arduino, without corrupting the peripheral state?

**Why it is uncertain:** The Arduino Wire driver on ESP32 wraps the IDF i2c
HAL. Mode switching via `end()` / `begin()` is not a documented use pattern.
It may work cleanly, leave the peripheral in a broken state, or behave
differently depending on timing.

**How we test it:** MCU #1 has two sender tasks that each call `send()` in a
loop. Every `send()` internally calls `_bus.end()` → master init → transmit →
`_bus.end()` → slave reinit → re-register ISR. MCU #2 receives and ACKs.
We observe whether transmissions succeed and ACKs arrive cleanly over many
cycles.

**Pass condition:** No `BusError::BUS_FAULT` on any `send()` call across at
least 20 cycles. ACKs arrive on MCU #1 receiver task. Serial shows clean
JSON on both sides.

**Fail condition:** Bus faults appear, transmissions stop after N cycles, or
the peripheral locks up requiring reboot.

**Failure path:** If mode switching is unreliable, the architecture must
change. Options to investigate: use two separate physical MCUs as permanent
master and permanent slave communicating via a dedicated master MCU; or
investigate whether a different I2C driver (e.g. direct IDF `i2c_master_*`
API) supports mode switching more cleanly.

---

### A2 — FreeRTOS queue passes messages without data loss

**What we are testing:** Does a FreeRTOS queue correctly hand a message from
the receiver task to the logic task on the same MCU, with no dropped messages
and no corruption?

**Why it is uncertain:** Queue depth, message size, and task priorities all
affect whether messages are dropped when the queue is full or the consumer
is slower than the producer.

**How we test it:** MCU #1 sends HEARTBEAT every 2s (Sender A) and every 3s
(Sender B). MCU #2 receiver task puts every received message on an inbound
queue. MCU #2 logic task drains the queue and prints each message with a
counter to serial.

**Pass condition:** The receive counter on MCU #2 matches the total send count
on MCU #1 after at least 10 full cycles of both senders. No
`xQueueSend` returns `errQUEUE_FULL`.

**Fail condition:** Counter mismatch between sent and received, or queue full
errors appear in serial output.

**Failure path:** Increase queue depth. If the logic task is too slow to drain
the queue, increase its priority or split processing into a separate task.

---

### A3 — Mutex prevents corruption under concurrent send attempts

**What we are testing:** When two tasks on the same MCU both try to acquire the
bus mutex simultaneously, does exactly one proceed while the other blocks, with
no garbled output on the wire?

**Why it is uncertain:** If the mutex is not correctly implemented or the
FreeRTOS scheduler switches tasks at an unfortunate moment, two tasks could
both enter the send path and produce overlapping I2C transmissions.

**How we test it:** MCU #1 runs two sender tasks (A at 2s, B at 3s). Their
intervals are chosen so they occasionally collide. MCU #2 logic task validates
every received JSON via `MessageProtocol::validate()`. Any garbled transmission
produces a parse or schema error, which is logged.

**Pass condition:** Zero `MessageProtocol::validate()` failures on MCU #2 after
at least 20 received messages. PulseView shows clean, non-overlapping I2C
transactions.

**Fail condition:** Validation errors appear on MCU #2, or PulseView shows
interleaved transactions.

**Failure path:** Review mutex take/give placement in `SharedBus::send()`.
Ensure the mutex is taken before `_bus.end()` and given only after slave
reinit completes. If FreeRTOS priorities are causing unexpected preemption,
raise the sender task priority.

---

## Task design

### MCU #1 tasks

| Task | Priority | Stack | Behaviour |
|---|---|---|---|
| Sender A | 2 | 4096 | Sends HEARTBEAT to MCU #2 every 2s via SharedBus::send() |
| Sender B | 2 | 4096 | Sends HEARTBEAT to MCU #2 every 3s via SharedBus::send() |
| Receiver | 3 | 2048 | Blocks on rxSemaphore, puts ACKs on inboundQueue |
| OLED | 1 | 2048 | Reads display state, updates OLED every 500ms |

### MCU #2 tasks

| Task | Priority | Stack | Behaviour |
|---|---|---|---|
| Receiver | 3 | 2048 | Blocks on rxSemaphore, puts messages on inboundQueue |
| Logic | 2 | 4096 | Drains inboundQueue, validates, sends HEARTBEAT_ACK |
| OLED | 1 | 2048 | Reads display state, updates OLED every 500ms |

### Shared globals (both MCUs, in main.cpp)

```cpp
QueueHandle_t    inboundQueue;   // receiver → logic
SemaphoreHandle_t busMutex;      // created inside SharedBus::init()
```

`busMutex` and `rxSemaphore` are created inside `SharedBus::init()` to
guarantee they exist before the ISR is registered.

---

## SharedBus changes required

- `init(uint8_t address)` — replaces separate `beginMaster()` / `beginSlave()`.
  Creates mutex and rxSemaphore, then calls `beginSlave(address)`.
- `send()` — internally: take mutex → `_bus.end()` → master init → transmit →
  slave reinit → re-register ISR → give mutex.
- `poll()` — replaced by ISR giving `rxSemaphore`; receiver task blocks on it.
- `_onReceiveISR` — calls `xSemaphoreGiveFromISR()` instead of setting
  `_rxReady = true`.

---

## Serial output expected

MCU #1:
```
[MCU1] SenderA → 0x09: OK  (#1)
[MCU1] SenderB → 0x09: OK  (#1)
[MCU1] Received HEARTBEAT_ACK from 0x09 (#1)
[MCU1] SenderA → 0x09: OK  (#2)
...
```

MCU #2:
```
[0x09] Received HEARTBEAT from 0x08 (#1)
[0x09] ACK → 0x08: OK
[0x09] Received HEARTBEAT from 0x08 (#2)
...
```

Any line containing `BUS_FAULT`, `Invalid`, or `parse failed` is a failure signal.

---

## Definition of done

All three assumptions pass. Results recorded in `results.md`. SharedBus and
main.cpp for MCU #1 and MCU #2 committed. Ready to proceed to Phase 3
full architecture design.
