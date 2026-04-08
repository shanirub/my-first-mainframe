#pragma once
#include <Wire.h>

enum class BusError { OK, NOT_FOUND, BUS_FAULT, TIMEOUT };

class SharedBus {
public:
    SharedBus();
    void beginMaster();
    void beginSlave(uint8_t address);
    BusError send(uint8_t targetAddress, const char* message);
    bool poll(char* buf, int bufLen);

private:
    TwoWire _bus;
    char _rxBuf[256];
    volatile int _rxLen;
    volatile bool _rxReady;

    static SharedBus* _instance;
    static void _onReceiveISR(int numBytes);
};
