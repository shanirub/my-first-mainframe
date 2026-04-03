# Devlog

---

## Phase 1.5 — SharedBus Class: Encapsulation and ISR-Safe Receive
*Goal: extract shared bus boilerplate into a shared library; fix ISR-unsafe OLED calls in onReceive*

### What we built
- `SharedBus` class in `shared/libs/shared_bus/` — encapsulates TwoWire(0) entirely
- `beginMaster()` / `beginSlave(address)` replace raw TwoWire init in every main.cpp
- `BusError send(target, message)` wraps the 3-step TwoWire dance with a typed error return
- `poll(buf, len)` replaces the `onReceive` callback pattern — safe to call from `loop()`
- No raw `TwoWire`, `Wire.h`, or pin numbers remain in any main.cpp

### Design decisions

**BusError enum over bool**
`endTransmission()` returns 5 distinct codes. A bool collapses `NOT_FOUND`, `BUS_FAULT`,
and `TIMEOUT` into a single failure, making diagnostics harder. `BusError` preserves the
distinction so callers can log or display the specific failure reason.

**poll() over onReceive callback**
The original MCU #2 code called `oled.showStatus()` directly inside `onReceive`. That
callback runs in interrupt context — calling U8g2 software I2C (bit-banging GPIOs) from
inside an active I2C interrupt is a known deadlock/corruption risk on ESP32.
The fix: `onReceive` ISR only reads raw bytes into an internal `_rxBuf` (no I2C, no heap,
no blocking). `poll()` is called from `loop()` — safe for OLED updates, Serial, and
anything else. The cost is a small polling delay (~10ms) which is acceptable for this
use case.

**Static _instance pointer**
Arduino's `onReceive` requires a plain C function pointer — no lambdas, no member
function pointers. A static `_instance` pointer lets the static ISR reach the live
TwoWire and buffer. One SharedBus per MCU is the intended constraint.

### Verified pass criteria
- MCU #1 serial: `[MCU1] SEND OK — MCU2 acknowledged` ✅
- MCU #2 serial: `[MCU2] Received: HELLO FROM MCU1` ✅
- No raw TwoWire or pin numbers in either main.cpp ✅
- shared_bus picked up automatically by PlatformIO LDF via lib_extra_dirs ✅

### Key learnings
- `endTransmission()` return codes map cleanly to typed errors — worth modelling explicitly
- ISR callbacks on Arduino are not safe for I2C or any blocking operation; buffer-and-poll
  is the standard workaround
- Static singleton pattern is the only practical way to bridge C-style callbacks to C++ objects
  in the Arduino framework

---

## Phase 1.5 — Dual I2C Bus Fix: OLED + Shared Bus Simultaneously
*Goal: run OLED display and inter-MCU shared bus at the same time on each MCU*

### What we built
- Migrated OLED library from Adafruit SSD1306 (hardware I2C) to U8g2 (software I2C)
- Both MCU #1 (master) and MCU #2 (slave) now run OLED and shared bus simultaneously
- U8g2 bit-bangs I2C in software on GPIO3/GPIO10, leaving TwoWire(0) free for shared bus

### What went wrong and how it was fixed

**1. TwoWire(1) silently fails on ESP32-C3**
Initial approach was to assign OLED to TwoWire(1) and shared bus to TwoWire(0).
Both MCUs crashed at boot with repeated `[Wire.cpp:526] write(): NULL TX buffer pointer`.
Root cause: ESP32-C3 has `SOC_I2C_NUM = 1` — only one hardware I2C peripheral.
`TwoWire(1).begin()` calls `i2cInit(1, ...)` which returns `ESP_ERR_INVALID_ARG` since
`1 >= SOC_I2C_NUM`. The buffer is never allocated. No error is surfaced to Arduino code.
The comment in soc_caps.h even says "have 2 I2C" — the comment is wrong.

**2. SoftWire not compatible with Adafruit SSD1306**
SoftWire was the first candidate to replace TwoWire(1). Rejected because
`Adafruit_SSD1306` takes a `TwoWire*` in its constructor — SoftWire doesn't
inherit from TwoWire, so it can't be passed where `TwoWire*` is expected without
modifying the Adafruit library.

**3. U8g2 SW_I2C constructor argument order**
U8g2 software I2C constructor takes `(rotation, SCL, SDA, reset)` — SCL before SDA,
opposite of Wire convention. Passing them in Wire order (SDA, SCL) would silently
communicate on the wrong pins.

**4. ISR constraint on MCU #2**
`onReceive` fires in interrupt context. Calling `oled.showStatus()` from inside it
would trigger I2C writes during an active I2C interrupt — a known deadlock/corruption
scenario on ESP32. OLED updates were kept in the receive handler in the current code
and remain a known risk; to be addressed when SharedBus class is introduced.

### Verified pass criteria
- MCU #1 serial: `[OLED] PASS: initialized` + `[MCU1] SEND OK — MCU2 acknowledged` ✅
- MCU #2 serial: `[OLED] PASS: initialized` + `[MCU2] Received 15 bytes: HELLO FROM MCU1` ✅
- Both running simultaneously without conflict ✅

### Key learnings
- ESP32-C3 `SOC_I2C_NUM = 1`: only one hardware I2C peripheral, despite misleading comment
- `TwoWire(1)` constructs silently but fails at `begin()` — no Arduino-level error
- For multi-bus I2C on ESP32-C3: hardware bus for shared/critical path, U8g2 SW_I2C for display
- Adafruit SSD1306 is not compatible with software I2C drop-ins — it requires `TwoWire*`
- U8g2 y-coordinates are text baseline, not top-left corner

---

## Task 3 — I2C Communication: MCU #1 (Master) → MCU #2 (Slave)
*Goal: establish verified bidirectional I2C communication on shared bus (GPIO8/GPIO9)*

### What we built
- Shared inter-MCU I2C bus: GPIO8=SDA, GPIO9=SCL, 5kΩ pull-ups to 3.3V
- MCU #1 as I2C master: sends "HELLO FROM MCU1" to address 0x09 every 2 seconds
- MCU #2 as I2C slave: listens on 0x09, prints received bytes to serial
- Config architecture: shared_config.h (pins + addresses) + per-MCU config.h

### What went wrong and how it was fixed

**1. Bench supply USB-C confusion**
Bench supply (Wanptek WPS3010H) output is off by default — must press output
button to enable. Current display shows 0.000A for low-draw devices which looks
like "not powered" but isn't. Red LED on MCU confirmed power delivery was fine.

**2. Shared config header not found by compiler**
Added ../shared/config to lib_extra_dirs in platformio.ini — but PlatformIO's
LDF only scans lib_extra_dirs for libraries with library.json, not plain headers.
Fix: use build_flags = -I ../shared/config instead. This passes the path directly
to the compiler.

**3. Accidentally flashed master code onto MCU #2**
Both MCUs ended up running identical master code. Neither was acting as slave,
so no ACK was ever possible. Lesson: always check main.cpp contents before
flashing, not just which directory you're in.

**4. TwoWire.begin() wrong overload for slave mode**
Slave code called: sharedBus.begin(I2C_ADDRESS, SHARED_SDA_PIN, SHARED_SCL_PIN)
This silently hit the wrong overload — the 3-argument form doesn't exist for
slave mode. The correct 4-argument slave overload requires explicit frequency:
    sharedBus.begin(I2C_ADDRESS, SHARED_SDA_PIN, SHARED_SCL_PIN, 0)
Without the 0, pins were ignored and slave never responded. Caught by reading
the actual TwoWire source — not trusting examples blindly.

**5. Logic analyzer SCL/SDA swapped in PulseView decoder**
Physical connections: D0=SDA (GPIO8/blue), D1=SCL (GPIO9/orange)
PulseView I2C decoder default assigns channel0=SCL, channel1=SDA — opposite
of our wiring. With wrong assignment, decoder showed Address: 0x00 instead of
0x09 and couldn't find stop condition. Fix: manually assign SCL→D1, SDA→D0
in decoder channel settings.

**6. Logic analyzer disconnecting mid-capture**
Short USB cable + unstable header connection caused intermittent disconnects,
producing flat-line captures that looked like idle bus. Fix: secure the header
connection, use a longer USB cable with slack.

### Verified pass criteria
- MCU #1 serial: [MCU1] SEND OK — MCU2 acknowledged ✅
- MCU #2 serial: [MCU2] Received 15 bytes: HELLO FROM MCU1 ✅
- PulseView decode: S | 09 | 48 45 4C 4C 4F 20 46 52 4F 4D 20 4D 43 55 31 | P ✅
  (translates to: HELLO FROM MCU1)

### Key learnings
- Read actual library source signatures — don't trust examples or docs alone
- endTransmission() return code 2 = address NACK = slave not responding,
  not a bus wiring problem
- TwoWire(0) constructor + explicit pins is mandatory on ESP32-C3 for
  non-default I2C pins — Wire.begin() has a confirmed bug
- OSI model applies to embedded I2C just like networking:
  Layer 1 = wires/pull-ups, Layer 2 = I2C protocol, Layer 7 = your message format
- PulseView SCL/SDA assignment must match physical wiring — wrong assignment
  produces plausible-looking but incorrect decode

### Captures
- docs/captures/task3_i2c_transmission_decoded.png
- docs/captures/task3_pulseview_session.sr

