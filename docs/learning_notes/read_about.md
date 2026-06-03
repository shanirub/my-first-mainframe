# ISR (freertos vs. espidf approach)

**The core problem both patterns solve:**

An ISR (Interrupt Service Routine — a function that runs when hardware signals an event) must be short and never block. But it needs to wake a sleeping task. FreeRTOS provides `xSemaphoreGiveFromISR()` for this — but after giving the semaphore, the scheduler needs to know whether to immediately switch to the newly-woken task, or finish the ISR and return to whatever was running before.

That decision requires `portYIELD_FROM_ISR`. The two patterns differ in **who calls it**.

---

**Pattern 1 — Raw FreeRTOS (you manage everything)**

```
Hardware event fires
        │
        ▼
┌─────────────────────────────────────────┐
│  your ISR function                      │
│                                         │
│  BaseType_t woken = pdFALSE;            │
│  xSemaphoreGiveFromISR(sem, &woken);    │◄── woken may become pdTRUE
│                                         │    if a task was waiting
│  portYIELD_FROM_ISR(woken);            │◄── YOU call this
│  // nothing after this line             │    triggers context switch
└─────────────────────────────────────────┘    if woken == pdTRUE
        │
        ▼
   ISR exits
        │
        ├─── woken == pdFALSE ──► return to interrupted task
        │
        └─── woken == pdTRUE  ──► switch to woken task immediately
```

You own the full sequence. `portYIELD_FROM_ISR` is the last thing that runs — nothing after it is guaranteed to execute.

---

**Pattern 2 — ESP-IDF callback (IDF manages the yield)**

```
Hardware event fires
        │
        ▼
┌─────────────────────────────────────────┐
│  IDF internal ISR handler               │
│  (registered by i2c_new_slave_device)   │
│                                         │
│  ... IDF reads hardware FIFO ...        │
│  ... copies to ringbuffer ...           │
│                                         │
│  bool woken = your_callback(...);      │◄── IDF calls YOUR function
│                                         │
│  portYIELD_FROM_ISR(woken);            │◄── IDF calls this, not you
└─────────────────────────────────────────┘
        │
        ▼
   ISR exits
        │
        ├─── woken == false ──► return to interrupted task
        │
        └─── woken == true  ──► switch to woken task immediately


        Your callback (on_receive):
        ┌─────────────────────────────────────────┐
        │  memcpy from evt_data->buffer           │
        │  _rxLen = evt_data->length              │
        │                                         │
        │  BaseType_t woken = pdFALSE;            │
        │  xSemaphoreGiveFromISR(sem, &woken);    │
        │                                         │
        │  return woken == pdTRUE;               │◄── hand result to IDF
        └─────────────────────────────────────────┘
```

Your callback does its work and returns a `bool`. IDF takes that bool and calls `portYIELD_FROM_ISR` itself, after your callback returns cleanly.

---

**Why IDF wraps it this way:**

IDF's internal ISR handler does work before calling your callback — reading the hardware FIFO, managing the ringbuffer. It needs to run `portYIELD_FROM_ISR` *after all of that*, not in the middle of it. If you called `portYIELD_FROM_ISR` inside your callback, you'd be yielding before IDF finishes its own cleanup. Handing the bool back lets IDF sequence things correctly.

---

**The key insight:**

Both patterns produce identical runtime behavior. The difference is purely about **who is responsible for calling `portYIELD_FROM_ISR`** and **when** it's safe to call it. In Pattern 1 you control the whole ISR. In Pattern 2 IDF controls the ISR and gives you a hook inside it.

---

## oled display code: new vs old driver

old driver uses c++ object member, new driver requires singleton

On the singleton pattern questione:
The other MCUs' `OledDisplay` wraps a `U8G2_SSD1306_128X64_NONAME_F_SW_I2C` C++ object directly as a member — u8g2's Arduino-style C++ API handles the callback internally. No singleton needed there.
For `OledDisplayWroom` we're using the C API with explicit callbacks — so we do need the bus handle and device handle accessible from a static callback. The singleton pattern from `SharedBusWroom` is the right approach.
