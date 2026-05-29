# Embedded / Firmware Glossary

Terms encountered in the MCU #3 bringup sessions.

---

**APB clock** — Advanced Peripheral Bus clock. One of the internal clock sources on ESP32, running at 80MHz. Used by peripherals including UART. arduino-esp32 2.x derived UART baud rates from APB; 3.x switched to XTAL.

**Auto-baud detection** — a UART receiver feature that measures incoming bit timing to determine the sender's baud rate automatically. CP2102 does not have this — it must be configured to exactly match the sender.

**Baud rate** — the number of signal changes per second on a serial line. For UART, this equals bits per second. Sender and receiver must agree on the same value or framing errors corrupt the data.

**Baud rate derivation** — the process of dividing a clock source frequency down to produce a target baud rate. If the clock frequency doesn't divide evenly into the target baud, there is a residual deviation. Large deviation causes framing errors at the receiver.

**Bit-banging** — implementing a hardware protocol (I2C, SPI, UART) entirely in software by toggling GPIO pins manually, without using a hardware peripheral. Slower and CPU-intensive, but avoids peripheral conflicts. U8g2 SW_I2C uses bit-banging.

**CP2102** — Silicon Labs USB-to-UART bridge IC. Converts USB data from a PC to UART signal levels (3.3V/5V). Used on the ESP32-WROOM-32 DevKit as the programming and monitor interface. Has no auto-baud — baud rate must match exactly.

**ESP-IDF** — Espressif IoT Development Framework. The official C SDK for ESP32 chips. Provides drivers, FreeRTOS integration, peripheral APIs, and the build system. arduino-esp32 is a layer on top of IDF.

**FIFO** — First In, First Out. A hardware queue built into a peripheral (UART TX, I2C TX). Data written to the FIFO is transmitted in order. "Pre-loading the TX FIFO" means writing response bytes into it before the master requests them, so they are ready to transmit immediately.

**FreeRTOS** — a real-time operating system kernel. Provides tasks (threads), queues, semaphores, and mutexes. Already running under the Arduino layer on ESP32 — you do not start it manually.

**GPIO matrix** — a hardware routing table inside ESP32 that maps peripheral signals (I2C SDA, UART TX, etc.) to physical GPIO pins. Allows any peripheral to be routed to almost any pin. `gpio_reset_pin()` clears a pin's entry in this table.

**gpio_reset_pin()** — IDF function that resets a GPIO pin to its default (floating input) state and removes it from the GPIO matrix, making it available for reassignment.

**I2C_NUM_0 / I2C_NUM_1** — identifiers for the two hardware I2C peripheral instances on ESP32-WROOM-32. Each is an independent hardware block. ESP32-C3 has only one (I2C_NUM_0).

**IDF component manager** — Espressif's dependency management tool for IDF components (analogous to npm for Node.js). Downloads and caches components listed in `idf_component.yml`. Stores them in `managed_components/` — auto-generated, not committed to git.

**interrupt** — a hardware signal that causes the CPU to immediately suspend the current task and execute a designated handler function (ISR). Used for time-sensitive events like I2C data arrival. The CPU resumes the previous task after the ISR returns.

**ISR** — Interrupt Service Routine. The function that runs when an interrupt fires. Must be short and fast — blocking FreeRTOS calls are forbidden. Uses `FromISR` variants of semaphore/queue APIs.

**pioarduino** — a community fork of the PlatformIO espressif32 platform that bundles arduino-esp32 3.x (IDF 5.5.x). Used for MCU #3 because IDF 5.2+ is required for `i2c_new_slave_device()`.

**platform (PlatformIO)** — the PlatformIO concept that bundles a toolchain, framework, and board support package for a specific chip family. `espressif32@7.0.1` bundles IDF 4.4.7. pioarduino bundles IDF 5.5.x.

**portMAX_DELAY** — a FreeRTOS constant meaning "wait forever." Passed as the timeout to blocking calls like `xSemaphoreTake()` or `xQueueReceive()` when the task should block indefinitely until the resource is available.

**pull-up resistor** — a resistor connected between a signal line and VCC. I2C requires pull-ups on SDA and SCL because the bus is open-drain — devices can only pull lines low, never high. The pull-up provides the high state passively.

**ring buffer** (also: circular buffer) — a fixed-size array treated as wrap-around. A write pointer advances on each write; a read pointer advances on each read. When either reaches the end, it wraps to index 0. Allows continuous data flow without allocation or shifting.

**sdkconfig** — the full generated configuration file for an IDF build. Contains hundreds of `CONFIG_*` keys. Generated from `sdkconfig.defaults` by the build system — do not commit it. Commit only `sdkconfig.defaults`.

**sdkconfig.defaults** — a sparse file containing only the `CONFIG_*` keys you want to override from IDF defaults. The build system merges this with IDF's full default config to produce `sdkconfig`.

**semaphore** — a FreeRTOS synchronization primitive. A binary semaphore has two states: given and taken. Used here to signal between an ISR (gives the semaphore) and a task (takes/blocks on the semaphore). `xSemaphoreGiveFromISR()` is the ISR-safe variant.

**smoke test** — the most basic possible test that a system doesn't immediately fail. Origin: powering on a new circuit and checking it doesn't literally smoke. Does not test correctness — only confirms basic operation.

**TG1WDT** — Timer Group 1 hardware watchdog. Monitors that each core's idle task gets CPU time at least once per second. Fires if any idle task is starved. On dual-core WROOM, a priority-20 task on Core 1 starves IDLE1 and triggers TG1WDT.

**TwoWire** — the Arduino class wrapping the I2C peripheral. `Wire` is TwoWire instance 0, `Wire1` is instance 1. On ESP32-WROOM-32, both map to real hardware peripherals. arduino-esp32 3.x auto-initializes `Wire` on GPIO8/9 before `setup()` runs.

**UART** — Universal Asynchronous Receiver/Transmitter. A serial communication protocol using two wires (TX, RX). Asynchronous means no shared clock — both sides must be configured to the same baud rate independently.

**XTAL** — crystal oscillator. The external crystal providing the base clock reference on ESP32. Runs at 40MHz on most ESP32 boards. arduino-esp32 3.x switched UART clock source from APB to XTAL, causing baud rate deviation at 115200 with CP2102.

**xTaskCreate()** — FreeRTOS function to create a new task (thread). Arguments include: function pointer, name, stack size (bytes on ESP32), parameters pointer, priority, and handle pointer. The task starts running immediately after creation if its priority is high enough.
