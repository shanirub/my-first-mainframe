┌─────────────────────────────────────┐
│         Your Application            │  main.cpp, tasks, business logic
├─────────────────────────────────────┤
│       Arduino Framework             │  Wire, Serial, pinMode, setup/loop
├─────────────────────────────────────┤
│     ESP-IDF Components              │  driver/i2c, driver/uart, driver/gpio
├─────────────────────────────────────┤
│   FreeRTOS (ESP32 port)             │  tasks, queues, semaphores, timers
├─────────────────────────────────────┤
│   HAL (Hardware Abstraction Layer)  │  esp32-hal-i2c.c, esp32-hal-gpio.c
│   ESP-IDF                           │  chip-specific register wrappers
├─────────────────────────────────────┤
│   ESP32 Hardware Peripherals        │  I2C, GPIO matrix, UART, SPI, timers
│   (memory-mapped registers)         │
├─────────────────────────────────────┤
│         Silicon                     │  Xtensa LX6 cores, physical chip
└─────────────────────────────────────┘