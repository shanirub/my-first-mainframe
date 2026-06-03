#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// SharedBusWroom — inter-MCU I2C bus driver for ESP32-WROOM-32 (MCU #3).
// Pure ESP-IDF, no Arduino layer. IDF 5.4.0 / espressif32 @ 6.10.0.
//
// Slave driver v2 (CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2=y):
//   - i2c_new_slave_device()           — creates slave on I2C_NUM_0, GPIO8/9
//   - i2c_slave_register_event_callbacks() — registers on_receive ISR
//   - i2c_slave_write()                — pre-loads TX ringbuffer (non-blocking)
//
// Receive path (ISR-driven):
//   on_receive callback fires → memcpy from IDF ringbuffer → give _rxSemaphore
//   poll() blocks on _rxSemaphore → copies _rxBuf to caller
//
// Transmit path:
//   send() calls i2c_slave_write() to load TX ringbuffer, returns immediately.
//   Master reads asynchronously — Logic task is never stalled.
//
// Threading:
//   poll()  — call from Receiver task only (single consumer of _rxSemaphore)
//   send()  — safe from any task (no shared mutable state, IDF driver is thread-safe)
//   init()  — call from app_main() before xTaskCreate()
//
// DO NOT include driver/i2c.h anywhere in MCU #3 — conflicts with i2c_slave.h.
// ─────────────────────────────────────────────────────────────────────────────

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/i2c_slave.h"
#include "driver/i2c_types.h"

// BusError — mirrors the enum used by SharedBus (ESP32-C3 version) for API
// compatibility. Logic task uses the same error handling regardless of MCU.
enum class BusError { OK, NOT_FOUND, BUS_FAULT, TIMEOUT };

class SharedBusWroom {
public:
    SharedBusWroom();

    // init() — configure and start I2C slave on I2C_NUM_0, GPIO8/9.
    // Creates _rxSemaphore, calls i2c_new_slave_device(), registers on_receive.
    // Must be called from app_main() before xTaskCreate() — semaphore must
    // exist before the first ISR can fire.
    void init(uint8_t address);

    // send() — pre-load TX ringbuffer via i2c_slave_write().
    // Non-blocking: returns as soon as bytes are in the ringbuffer.
    // Master reads asynchronously. Safe to call from any task.
    // targetAddress is ignored (slave has no control over which master reads) —
    // kept for API compatibility with SharedBus.
    BusError send(uint8_t targetAddress, const char* message);

    // poll() — block calling task until a message arrives from the master.
    // Copies received bytes into buf (max bufLen-1 bytes + '\0').
    // Returns true when a message is available, false on internal error.
    // Call from Receiver task only.
    bool poll(char* buf, int bufLen);

private:
    i2c_slave_dev_handle_t _handle;
    uint8_t                _address;

    // _rxBuf: written by on_receive ISR via memcpy, read by poll().
    // _rxLen: byte count written by ISR.
    // Protected implicitly: ISR writes before giving semaphore,
    // poll() reads after taking semaphore — happens-before is guaranteed.
    char             _rxBuf[256];
    uint32_t         _rxLen;
    SemaphoreHandle_t _rxSemaphore;

    // IDF requires a plain C function pointer for the callback.
    // _instance holds the singleton pointer so the static callback can
    // reach member state. Safe because exactly one SharedBusWroom exists.
    static SharedBusWroom* _instance;

    // on_receive — ISR callback, called by IDF when master writes to us.
    // Must be static (plain function pointer). Returns bool: whether a
    // higher-priority task was woken (IDF calls portYIELD_FROM_ISR with it).
    // Signature verified against i2c_slave_received_callback_t in i2c_types.h.
    static bool IRAM_ATTR on_receive(
        i2c_slave_dev_handle_t handle,
        const i2c_slave_rx_done_event_data_t* evt_data,
        void* arg
    );
};

// ─────────────────────────────────────────────────────────────────────────────
// ReceiverParamsWroom — passed to receiverTaskWroom via xTaskCreate pvParameters.
// Declare as file-scope static in main.cpp — do NOT stack-allocate (the task
// outlives the stack frame that created it).
// ─────────────────────────────────────────────────────────────────────────────
struct ReceiverParamsWroom {
    SharedBusWroom* bus;
    QueueHandle_t   inboundQueue;
};

// receiverTaskWroom — blocks on bus->poll(), forwards each message to
// inboundQueue for the Logic task. Same pattern as receiverTask() on C3 MCUs.
void receiverTaskWroom(void* params);