#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// SharedBusWroom — shared inter-MCU I2C bus implementation for ESP32-WROOM-32.
//
// Uses IDF 5.x i2c_new_slave_device() (driver/i2c_slave.h) with
// CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2=y (push-based receive, non-blocking
// transmit). Avoids the TG1WDT caused by Arduino Wire slave on dual-core
// ESP32-WROOM-32. See ADR-010 for full root cause and decision chain.
//
// Interface mirrors SharedBus (shared_bus.h) used by ESP32-C3 MCUs:
//   init(), send(), poll()
// Selected at compile time via #ifdef MCU_BOARD_WROOM in consuming code.
//
// Receive path:
//   on_receive ISR → copies bytes to _rxBuf, gives _rxSemaphore
//   poll() blocks on _rxSemaphore, copies _rxBuf to caller
//
// Transmit path:
//   send() calls i2c_slave_write() to pre-load TX FIFO, returns immediately.
//   Master reads from FIFO asynchronously — logic task is never stalled.
//
// Threading:
//   poll() must be called from the receiver task only.
//   send() is safe to call from any task — internal critical section protects
//   _rxBuf from concurrent access between ISR and poll().
//
// DO NOT include driver/i2c.h (old driver) anywhere in the MCU #3 build —
// it conflicts with driver/i2c_slave.h at link time.
// ─────────────────────────────────────────────────────────────────────────────

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include "driver/i2c_slave.h"

enum class BusError { OK, NOT_FOUND, BUS_FAULT, TIMEOUT };

class SharedBusWroom {
public:
    SharedBusWroom();

    // Creates _rxSemaphore, configures i2c_slave_config_t, calls
    // i2c_new_slave_device(), registers on_receive callback.
    // Must be called before xTaskCreate() — semaphore must exist
    // before the ISR can fire.
    void init(uint8_t address);

    // Pre-loads TX FIFO via i2c_slave_write() so master can read on demand.
    // Non-blocking — returns immediately after loading. Safe from any task.
    BusError send(uint8_t targetAddress, const char* message);

    // Blocks calling task on _rxSemaphore until ISR signals a message.
    // Copies message to buf. Call from receiver task only.
    bool poll(char* buf, int bufLen);

private:
    i2c_slave_dev_handle_t _handle;
    uint8_t                _address;

    // _rxBuf written by ISR (on_receive), read by poll().
    // _rxLen set by ISR. Protected by _rxMutex between ISR write and poll read.
    char             _rxBuf[256];
    volatile int     _rxLen;
    SemaphoreHandle_t _rxSemaphore;

    // Static ISR callback — IDF requires a plain function pointer.
    // Uses instance pointer stored in _instance to reach member state.
    static SharedBusWroom* _instance;
    static bool _onReceive(
        i2c_slave_dev_handle_t handle,
        const i2c_slave_rx_done_event_data_t* evt_data,
        void* arg
    );
};

// ─────────────────────────────────────────────────────────────────────────────
// ReceiverParamsWroom — passed to receiverTask via xTaskCreate pvParameters.
// Must be file-scope static in main.cpp — do NOT stack-allocate in setup().
// ─────────────────────────────────────────────────────────────────────────────
struct ReceiverParamsWroom {
    SharedBusWroom* bus;
    QueueHandle_t   queue;
};

// Shared FreeRTOS receiver task — same pattern as receiverTask() in shared_bus.
// Blocks on bus->poll(), forwards each message to queue.
void receiverTaskWroom(void* params);
