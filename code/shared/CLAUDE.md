# Shared Code

## Structure
shared/
  libs/
    oled_display/    ← U8g2 SW_I2C wrapper, used by all MCUs
    shared_bus/      ← inter-MCU I2C bus library, used by all MCUs
  config/
    shared_config.h  ← single source of truth for all pins and addresses

## shared_config.h — What Lives Here
- SHARED_SDA_PIN / SHARED_SCL_PIN: inter-MCU bus pins (GPIO8/GPIO9)
- OLED_SDA_PIN / OLED_SCL_PIN: private OLED bus pins (GPIO3/GPIO10)
- SHARED_I2C_FREQ: 400000 (400kHz Fast Mode)
- ADDR_* constants for all 5 MCU addresses (0x08–0x0C)

## oled_display library
- Wraps U8g2 with a clean interface: begin(), showStatus(), showError()
- Uses U8g2 software I2C (bit-bang) on OLED_SDA_PIN / OLED_SCL_PIN
- Required because ESP32-C3 has only one hardware I2C peripheral (SOC_I2C_NUM=1).
  TwoWire(1) silently fails — U8g2 SW_I2C avoids the conflict entirely.
- Replaced Adafruit SSD1306 (which required TwoWire* and was incompatible with
  software I2C drop-ins)
- Pins passed in constructor — no hardcoded values in library code

## shared_bus library
- Encapsulates TwoWire(0) for the inter-MCU shared I2C bus
- Public interface:
    beginMaster()                              — master mode init, pins from shared_config.h
    beginSlave(uint8_t address)               — slave mode init
    BusError send(uint8_t target, const char* msg) — master transmit, typed error return
    bool poll(char* buf, int bufLen)           — safe receive, call from loop()
- BusError enum: OK, NOT_FOUND, BUS_FAULT, TIMEOUT — maps endTransmission() codes
- ISR/poll() architecture: onReceive ISR only reads bytes into internal _rxBuf.
  All user code (OLED, Serial) runs in poll(), which is called from loop().
  This prevents calling I2C from inside an active I2C interrupt (deadlock risk).
- Static _instance pointer allows the static ISR to reach the TwoWire instance.
  Only one SharedBus should exist per MCU.

## Rules
- shared_config.h must never include MCU-specific logic
- oled_display and shared_bus libraries must never hardcode pin numbers
- Every new shared library needs a library.json file
- lib_extra_dirs = ../shared/libs covers all libraries under shared/libs automatically