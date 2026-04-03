#include "shared_bus.h"
#include "shared_config.h"
#include <string.h>

SharedBus* SharedBus::_instance = nullptr;

SharedBus::SharedBus()
    : _bus(0), _rxLen(0), _rxReady(false) {}

void SharedBus::beginMaster() {
    _bus.begin(SHARED_SDA_PIN, SHARED_SCL_PIN);
    _bus.setClock(SHARED_I2C_FREQ);
}

void SharedBus::beginSlave(uint8_t address) {
    // Store instance pointer so the static ISR callback can reach _bus and _rxBuf.
    // Only one SharedBus instance should exist per MCU.
    _instance = this;
    _bus.begin(address, SHARED_SDA_PIN, SHARED_SCL_PIN, 0);
    _bus.onReceive(_onReceiveISR);
}

// Static ISR — runs in interrupt context. Only reads from _bus into _rxBuf;
// no I2C calls, no heap allocation, no blocking operations.
void SharedBus::_onReceiveISR(int numBytes) {
    if (_instance == nullptr) return;
    int i = 0;
    while (_instance->_bus.available() && i < (int)sizeof(_instance->_rxBuf) - 1) {
        _instance->_rxBuf[i++] = _instance->_bus.read();
    }
    _instance->_rxBuf[i] = '\0';
    _instance->_rxLen = i;
    _instance->_rxReady = true;
}

BusError SharedBus::send(uint8_t targetAddress, const char* message) {
    _bus.beginTransmission(targetAddress);
    _bus.write(message);
    uint8_t result = _bus.endTransmission();
    switch (result) {
        case 0: return BusError::OK;
        case 2: return BusError::NOT_FOUND;   // address NACK — slave absent
        case 3: return BusError::BUS_FAULT;   // data NACK
        case 4: return BusError::BUS_FAULT;   // other error
        case 5: return BusError::TIMEOUT;
        default: return BusError::BUS_FAULT;
    }
}

// Call from loop() — safe to do I2C, OLED, or Serial work here.
// Returns true and fills buf if a message arrived since last poll.
bool SharedBus::poll(char* buf, int bufLen) {
    if (!_rxReady) return false;
    int len = (_rxLen < bufLen - 1) ? _rxLen : bufLen - 1;
    memcpy(buf, _rxBuf, len);
    buf[len] = '\0';
    _rxReady = false;
    return true;
}
