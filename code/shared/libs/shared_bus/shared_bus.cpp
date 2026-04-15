#include "shared_bus.h"
#include "shared_config.h"
#include <Arduino.h>
#include <string.h>

SharedBus* SharedBus::_instance = nullptr;

SharedBus::SharedBus()
    : _bus(0), _address(0), _rxLen(0),
      busMutex(nullptr), _rxSemaphore(nullptr) {}

// ─────────────────────────────────────────────────────────────────────────────
// init
//
// Call once in setup(), before xTaskCreate().
// Creates busMutex and _rxSemaphore first — both must exist before the ISR
// is registered, because the ISR gives _rxSemaphore on the first received byte.
// ─────────────────────────────────────────────────────────────────────────────
void SharedBus::init(uint8_t address) {
    _address = address;
    _instance = this;

    busMutex     = xSemaphoreCreateMutex();
    _rxSemaphore = xSemaphoreCreateBinary();

    if (busMutex == nullptr || _rxSemaphore == nullptr) {
        Serial.println("[SharedBus] FATAL: failed to create semaphores");
        return;
    }

    _switchToSlave();
    Serial.printf("[SharedBus] init complete, slave addr=0x%02X\n", _address);
}

// ─────────────────────────────────────────────────────────────────────────────
// send
//
// Takes busMutex, switches to master, transmits, switches back to slave,
// gives busMutex. Any task can call this — the mutex serialises access.
// ─────────────────────────────────────────────────────────────────────────────
BusError SharedBus::send(uint8_t targetAddress, const char* message) {
    xSemaphoreTake(busMutex, portMAX_DELAY);

    _switchToMaster();

    _bus.beginTransmission(targetAddress);
    _bus.write((const uint8_t*)message, strlen(message));
    uint8_t result = _bus.endTransmission();

    _switchToSlave();

    xSemaphoreGive(busMutex);

    switch (result) {
        case 0:  return BusError::OK;
        case 2:  return BusError::NOT_FOUND;
        case 3:  return BusError::BUS_FAULT;
        case 4:  return BusError::BUS_FAULT;
        case 5:  return BusError::TIMEOUT;
        default: return BusError::BUS_FAULT;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// poll
//
// Blocks the calling task on _rxSemaphore until the ISR gives it.
// Returns true and copies the buffer when a message has arrived.
// Call from the receiver task only.
// ─────────────────────────────────────────────────────────────────────────────
bool SharedBus::poll(char* buf, int bufLen) {
    // Block indefinitely until ISR signals a message arrived.
    if (xSemaphoreTake(_rxSemaphore, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    int len = (_rxLen < bufLen - 1) ? _rxLen : bufLen - 1;
    memcpy(buf, _rxBuf, len);
    buf[len] = '\0';
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// _switchToMaster  (private, called inside send() while mutex is held)
// ─────────────────────────────────────────────────────────────────────────────
void SharedBus::_switchToMaster() {
    _bus.end();
    _bus.begin(SHARED_SDA_PIN, SHARED_SCL_PIN);
    _bus.setClock(SHARED_I2C_FREQ);
}

// ─────────────────────────────────────────────────────────────────────────────
// _switchToSlave  (private, called from init() and inside send())
// Re-registers the ISR each time — required after every begin() call.
// ─────────────────────────────────────────────────────────────────────────────
void SharedBus::_switchToSlave() {
    _bus.end();
    _bus.begin(_address, SHARED_SDA_PIN, SHARED_SCL_PIN, 0);
    _bus.onReceive(_onReceiveISR);
}

// ─────────────────────────────────────────────────────────────────────────────
// _onReceiveISR  (static, runs in interrupt context)
//
// Reads bytes into _rxBuf and gives _rxSemaphore to wake the receiver task.
// Must not call any blocking FreeRTOS API — uses FromISR variants only.
// No I2C calls, no Serial, no heap allocation.
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// receiverTask  (free function — shared across all MCUs)
//
// Blocks on bus->poll() which blocks on rxSemaphore until the ISR fires.
// Forwards each raw message buffer to the inboundQueue for the logic task.
// No JSON, no Serial — keep this hot path minimal.
// ─────────────────────────────────────────────────────────────────────────────
void receiverTask(void* params) {
    ReceiverParams* p = static_cast<ReceiverParams*>(params);
    char buf[256];
    while (true) {
        if (p->bus->poll(buf, sizeof(buf))) {
            if (xQueueSend(p->queue, buf, portMAX_DELAY) != pdTRUE) {
                Serial.println("[RX] queue full — message dropped");
            }
        }
    }
}

void SharedBus::_onReceiveISR(int numBytes) {
    if (_instance == nullptr) return;

    int i = 0;
    while (_instance->_bus.available() &&
           i < (int)sizeof(_instance->_rxBuf) - 1) {
        _instance->_rxBuf[i++] = _instance->_bus.read();
    }
    _instance->_rxBuf[i] = '\0';
    _instance->_rxLen    = i;

    // Wake the receiver task. pdFALSE = no context switch requested here;
    // the scheduler will switch after the ISR returns if a higher-priority
    // task was unblocked.
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(_instance->_rxSemaphore, &higherPriorityTaskWoken);
    if (higherPriorityTaskWoken) portYIELD_FROM_ISR();
}