# Shared Code

## Structure
shared/
  libs/
    oled_display/    ← Adafruit SSD1306 wrapper, used by all MCUs
  config/
    shared_config.h  ← single source of truth for all pins and addresses

## shared_config.h — What Lives Here
- SHARED_SDA_PIN / SHARED_SCL_PIN: inter-MCU bus pins (GPIO8/GPIO9)
- OLED_SDA_PIN / OLED_SCL_PIN: private OLED bus pins (GPIO3/GPIO10)
- SHARED_I2C_FREQ: 400000 (400kHz Fast Mode)
- ADDR_* constants for all 5 MCU addresses (0x08–0x0C)

## oled_display library
- Wraps Adafruit SSD1306 with a clean interface: begin(), showStatus(), showError()
- Uses TwoWire(0) internally — safe for ESP32-C3 non-default pins
- Pins passed in as constructor arguments — no hardcoded pins in library code
- Requires library.json to be recognised by PlatformIO

## Rules
- shared_config.h must never include MCU-specific logic
- oled_display library must never hardcode pin numbers
- Every new shared library needs a library.json file
- Adding a new shared folder requires adding it to lib_extra_dirs
  in every MCU's platformio.ini that uses it