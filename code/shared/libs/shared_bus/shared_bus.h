#pragma once

#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

enum class BusError { OK, NOT_FOUND, BUS_FAULT, TIMEOUT };

class SharedBus {
public:
    SharedBus();

    // Replaces beginMaster() and beginSlave().
    // Creates busMutex and rxSemaphore, then initialises TwoWire(0) in slave
    // mode. Must be called before xTaskCreate() so the semaphore exists before
    // the ISR can fire.
    void init(uint8_t address);

    // Acquires busMutex, switches to master, transmits, switches back to slave,
    // re-registers ISR, releases busMutex. Safe to call from any task.
    BusError send(uint8_t targetAddress, const char* message);

    // Returns true and fills buf if rxSemaphore has been given by the ISR.
    // Blocks the calling task until a message arrives — no CPU is consumed
    // while waiting. Call from the receiver task only.
    bool poll(char* buf, int bufLen);

    // FreeRTOS handle exposed so main.cpp can pass it to tasks that need
    // to acquire the bus mutex directly (none in this PoC, but available).
    SemaphoreHandle_t busMutex;

private:
    TwoWire          _bus;
    uint8_t          _address;
    char             _rxBuf[256];
    volatile int     _rxLen;
    SemaphoreHandle_t _rxSemaphore;

    static SharedBus* _instance;
    static void _onReceiveISR(int numBytes);

    // Internal helpers called inside send() while mutex is held.
    void _switchToMaster();
    void _switchToSlave();
};

// ─────────────────────────────────────────────────────────────────────────────
// ReceiverParams — passed to receiverTask via xTaskCreate pvParameters.
//
// The struct MUST be a file-scope static in main.cpp — do NOT allocate it
// inside setup(). setup() returns after task creation; a stack-allocated
// struct would be destroyed, leaving the task with a dangling pointer.
// ─────────────────────────────────────────────────────────────────────────────
struct ReceiverParams {
    SharedBus*    bus;
    QueueHandle_t queue;
};

// Shared FreeRTOS receiver task.
// Blocks on bus->poll(), then forwards each message to queue.
// Pass a static ReceiverParams* as pvParameters to xTaskCreate.
void receiverTask(void* params);