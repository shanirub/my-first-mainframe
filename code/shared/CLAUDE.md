# Shared Code

## Structure
shared/
  libs/
    oled_display/    ← U8g2 SW_I2C wrapper, used by all MCUs
    shared_bus/      ← inter-MCU I2C bus library, FreeRTOS task-safe
    message_protocol/ ← JSON envelope, validation, constants
  config/
    shared_config.h  ← single source of truth for pins, addresses, stack sizes

## shared_config.h — What Lives Here
- SHARED_SDA_PIN / SHARED_SCL_PIN: inter-MCU bus pins (GPIO8/GPIO9)
- OLED_SDA_PIN / OLED_SCL_PIN: private OLED bus pins (GPIO3/GPIO10)
- SHARED_I2C_FREQ: 400000 (400kHz Fast Mode)
- ADDR_* constants for all 5 MCU addresses (0x08–0x0C)
- STACK_SIZE_SENDER, STACK_SIZE_RECEIVER, STACK_SIZE_LOGIC, STACK_SIZE_OLED
  (bytes, not words — ESP32 xTaskCreate takes bytes)

## shared_bus library (FreeRTOS — current API)
Encapsulates TwoWire(0) for the inter-MCU shared I2C bus.

Public interface:
    init(uint8_t address)                     — replaces beginMaster()/beginSlave().
                                                Creates busMutex + rxSemaphore,
                                                initialises as slave, registers ISR.
                                                Must be called before xTaskCreate().
    BusError send(uint8_t target, const char* msg) — task-safe send. Internally:
                                                take mutex → end() → master init →
                                                transmit → end() → slave reinit →
                                                re-register ISR → give mutex.
    bool poll(char* buf, int len)             — blocks calling task on rxSemaphore.
                                                Returns true when message arrives.
                                                Call from receiver task only.
    SemaphoreHandle_t busMutex               — public, exposed if needed externally.

ISR pattern: onReceive ISR fills _rxBuf and calls xSemaphoreGiveFromISR()
with portYIELD_FROM_ISR(). Receiver task blocks on rxSemaphore — zero CPU
while waiting, wakes in same tick as ISR if receiver has higher priority.

Do NOT call vTaskStartScheduler() anywhere — ESP32 Arduino starts FreeRTOS
before setup(). Use vTaskDelete(NULL) in loop() to reclaim loopTask stack.

BusError enum: OK, NOT_FOUND, BUS_FAULT, TIMEOUT — maps endTransmission() codes.

## oled_display library
- Wraps U8g2 with a clean interface: begin(), showStatus(), showError()
- Uses U8g2 software I2C (bit-bang) on OLED_SDA_PIN / OLED_SCL_PIN
- Required because ESP32-C3 has only one hardware I2C peripheral (SOC_I2C_NUM=1)
- Unchanged by FreeRTOS migration — called from OLED task, not from ISR

## message_protocol library
- JSON envelope builder, schema validation, MsgType/TxnType/Status/Priority constants
- All methods static — no instance required
- Unchanged by FreeRTOS migration
- See libs/message_protocol/API_REFERENCE.md for full documentation

## Rules
- shared_config.h must never include MCU-specific logic
- oled_display and shared_bus libraries must never hardcode pin numbers
- Every new shared library needs a library.json file
- lib_extra_dirs = ../shared/libs covers all libraries under shared/libs automatically
- shared_config.h included via build_flags = -I ../shared/config (not lib_extra_dirs)
