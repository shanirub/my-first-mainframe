## Glossary

**TG1WDT** — Timer Group 1 hardware watchdog. Monitors that each core's idle task gets CPU time at least once per second. Fires if any idle task is starved.

**Idle task (IDLE0/IDLE1)** — Lowest-priority FreeRTOS task, one per core. Runs only when no other task is ready. WDT monitors it as a health signal.

**loopTask** — Arduino's main task wrapping `setup()` and `loop()`. Runs on Core 1 on ESP32-WROOM-32.

**i2cSlaveInit()** — Internal Arduino Wire function that creates `i2c_slave_task` at priority 20 — higher than all user tasks — causing IDLE1 starvation on dual-core ESP32.

**Open-drain** — Electrical configuration where a device can only pull a wire LOW or release it. Wire goes HIGH passively via pull-up resistor. Fundamental to I2C — multiple devices share SDA safely.

**Pull-up resistor** — Resistor connecting a signal line to VCC. Biases the line HIGH when nothing actively drives it low. Required on I2C SDA and SCL lines.

**Pull-down resistor** — Resistor connecting a signal line to GND. Biases the line LOW when nothing drives it.

**Bit-banging** — Software simulation of a hardware protocol by manually toggling GPIO pins. U8g2 uses this for software I2C. CPU-intensive, no hardware offloading.

**Spin-loop (busy-wait)** — Code that repeatedly checks a condition without yielding. Example: waiting for UART TX FIFO space. Blocks the CPU entirely.

**UART TX FIFO** — Hardware queue for outgoing serial bytes. Limited size. `uart_write()` spin-loops when full, blocking the task.

**Hardware abstraction layer (HAL)** — Layer of code isolating hardware-specific details from application logic. Same interface, different implementations per chip.

**Bridge pattern** — Software design pattern separating abstraction from implementation so both can vary independently. Used here: same SharedBus interface, different WROOM/C3 implementations.

**Conditional compilation** — Using `#ifdef`/`#ifndef` preprocessor directives to include different code paths at compile time based on build flags (e.g. `MCU_BOARD_WROOM`).

**`esp_log_level_set()`** — ESP-IDF runtime function to set log verbosity per tag. `esp_log_level_set("gpio", ESP_LOG_WARN)` suppresses INFO messages from the GPIO driver.

**`i2c_driver_install()`** — IDF 4.4.7 I2C driver init function. Also creates internal tasks — same WDT problem as Arduino Wire on WROOM dual-core.

**`i2c_new_slave_device()`** — IDF 5.x+ new I2C slave API. Interrupt-driven, no internal task creation. Requires `framework = espidf` to access.

**TwoWire** — Arduino ESP32 class implementing I2C. `Wire` = `TwoWire(0)`, `Wire1` = `TwoWire(1)`. Pins assigned at `begin()` time.

**`sdkconfig.defaults`** — Minimal ESP-IDF project config overrides file. Source of truth. Auto-generates full `sdkconfig.esp32dev` at build time. Only custom lines go here.

**Combined framework mode** — `framework = arduino, espidf` in platformio.ini. Keeps Arduino APIs while exposing ESP-IDF internals. Still bundles IDF 4.4.7 with Arduino framework.

**Context save (`_xt_context_save`)** — Xtensa CPU routine saving registers during FreeRTOS task switch. Appearing in WDT backtrace indicates WDT fired during task creation/switching.

**`vTaskDelay()`** — FreeRTOS call that blocks a task for N ticks, removing it from the ready list. Allows idle task to run. Not the same as `vTaskYield()`.

**`vTaskYield()`** — FreeRTOS call that passes execution to the next ready task at same or higher priority. Does not guarantee idle task runs.

**Core affinity** — FreeRTOS concept of pinning a task to a specific CPU core via `xTaskCreatePinnedToCore()`. Default tasks use `tskNO_AFFINITY`.

**U8g2** — Display driver library supporting many OLED/LCD controllers. Handles SSD1306 protocol, font rendering, framebuffer. Supports both software and hardware I2C transport.

**Hardware I2C (U8g2)** — U8g2 mode using the ESP32 hardware I2C peripheral instead of bit-banging. Configured via `U8G2_SSD1306_128X64_NONAME_F_HW_I2C` constructor.
