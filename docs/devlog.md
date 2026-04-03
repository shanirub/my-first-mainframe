# Devlog

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

