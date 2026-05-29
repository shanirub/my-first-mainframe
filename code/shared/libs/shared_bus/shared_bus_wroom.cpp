#include "shared_bus_wroom.h"
#include "shared_config.h"
#include <Arduino.h>
#include <string.h>

SharedBusWroom* SharedBusWroom::_instance = nullptr;

SharedBusWroom::SharedBusWroom()
    : _handle(nullptr), _address(0), _rxLen(0),
      _rxSemaphore(nullptr) {}

// ─────────────────────────────────────────────────────────────────────────────
// init
//
// Call once in setup(), before xTaskCreate().
// Creates _rxSemaphore first — must exist before ISR can fire.
// Configures and installs the IDF 5.x slave device on I2C_NUM_0 (GPIO8/9).
// Registers on_receive callback (Version 2 push-based receive).
// ─────────────────────────────────────────────────────────────────────────────
void SharedBusWroom::init(uint8_t address) {
    _address  = address;
    _instance = this;

    _rxSemaphore = xSemaphoreCreateBinary();
    if (_rxSemaphore == nullptr) {
        Serial.println("[SharedBusWroom] FATAL: failed to create semaphore");
        return;
    }

    // i2c_slave_config_t — IDF 5.x new slave driver configuration struct.
    // i2c_port = -1 means auto-select the first available I2C peripheral.
    // send_buf_depth — TX ringbuffer depth in bytes; 256 matches our max message size.
    // receive_buf_depth — internal RX software buffer; 512 gives two full messages
    //   of headroom before the ISR callback fires, reducing drop risk under burst traffic.
    // enable_internal_pullup — enabled as safe default; external 5kΩ pull-ups on hub
    //   dominate and this has no harmful effect.
    i2c_slave_config_t conf = {};
    conf.i2c_port          = I2C_NUM_1;
    conf.sda_io_num        = (gpio_num_t)SHARED_SDA_PIN;
    conf.scl_io_num        = (gpio_num_t)SHARED_SCL_PIN;
    conf.clk_source        = I2C_CLK_SRC_DEFAULT;
    conf.send_buf_depth    = 256;
    conf.receive_buf_depth = 512;
    conf.slave_addr        = _address;
    conf.addr_bit_len      = I2C_ADDR_BIT_LEN_7;
    conf.intr_priority     = 0;  // let driver select default (1-3)
    conf.flags.enable_internal_pullup = 1;

    esp_err_t err = i2c_new_slave_device(&conf, &_handle);
    if (err != ESP_OK) {
        Serial.printf("[SharedBusWroom] FATAL: i2c_new_slave_device failed: %s\n",
            esp_err_to_name(err));
        return;
    }

    // Register on_receive callback.
    // on_request is not registered — we pre-load TX FIFO in send() before
    // the master reads, so the FIFO-empty condition should not occur in normal
    // operation. If it does, the master receives 0xFF (bus default).
    i2c_slave_event_callbacks_t cbs = {};
    cbs.on_receive = _onReceive;

    err = i2c_slave_register_event_callbacks(_handle, &cbs, this);
    if (err != ESP_OK) {
        Serial.printf("[SharedBusWroom] FATAL: register callbacks failed: %s\n",
            esp_err_to_name(err));
        return;
    }

    Serial.printf("[SharedBusWroom] init complete, slave addr=0x%02X SDA=GPIO%d SCL=GPIO%d\n",
        _address, SHARED_SDA_PIN, SHARED_SCL_PIN);
}

// ─────────────────────────────────────────────────────────────────────────────
// send
//
// Pre-loads TX FIFO via i2c_slave_write() so the master can read on demand.
// Non-blocking — returns immediately after the FIFO is loaded.
// targetAddress is unused (slave cannot address master) but kept for
// interface parity with SharedBus used by C3 MCUs.
// ─────────────────────────────────────────────────────────────────────────────
BusError SharedBusWroom::send(uint8_t targetAddress, const char* message) {
    (void)targetAddress;  // slave cannot initiate; master reads our TX FIFO

    if (_handle == nullptr) return BusError::BUS_FAULT;

    uint32_t written = 0;
    esp_err_t err = i2c_slave_write(
        _handle,
        (const uint8_t*)message,
        (uint32_t)strlen(message),
        &written,
        0   // timeout_ms=0: non-blocking, load what fits in FIFO and return
    );

    if (err != ESP_OK) {
        Serial.printf("[SharedBusWroom] send failed: %s\n", esp_err_to_name(err));
        return BusError::BUS_FAULT;
    }
    return BusError::OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// poll
//
// Blocks the calling task on _rxSemaphore until the ISR gives it.
// Copies _rxBuf to caller's buffer. Call from receiver task only.
// ─────────────────────────────────────────────────────────────────────────────
bool SharedBusWroom::poll(char* buf, int bufLen) {
    if (xSemaphoreTake(_rxSemaphore, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    int len = (_rxLen < bufLen - 1) ? _rxLen : bufLen - 1;
    memcpy(buf, _rxBuf, len);
    buf[len] = '\0';
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// _onReceive  (static, runs in ISR context)
//
// Called by IDF driver when a complete I2C transaction is received.
// evt_data->buffer — pointer to received bytes (valid only during callback).
// evt_data->length — byte count.
//
// Copies to _rxBuf immediately — buffer is only valid for callback duration.
// Gives _rxSemaphore to wake receiver task.
// Must not call any blocking FreeRTOS API — uses FromISR variants only.
// No Serial, no heap allocation, no Wire calls.
//
// Returns true if a higher-priority task was woken — IDF uses this to
// decide whether to yield after the ISR returns.
// ─────────────────────────────────────────────────────────────────────────────
bool SharedBusWroom::_onReceive(
    i2c_slave_dev_handle_t handle,
    const i2c_slave_rx_done_event_data_t* evt_data,
    void* arg
) {
    (void)handle;
    (void)arg;

    if (_instance == nullptr || evt_data == nullptr) return false;

    uint32_t len = evt_data->length;
    if (len > sizeof(_instance->_rxBuf) - 1) {
        len = sizeof(_instance->_rxBuf) - 1;
    }

    memcpy(_instance->_rxBuf, evt_data->buffer, len);
    _instance->_rxBuf[len] = '\0';
    _instance->_rxLen      = (int)len;

    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(_instance->_rxSemaphore, &higherPriorityTaskWoken);
    return (higherPriorityTaskWoken == pdTRUE);
}

// ─────────────────────────────────────────────────────────────────────────────
// receiverTaskWroom  (free function)
//
// Mirrors receiverTask() in shared_bus.cpp.
// Blocks on bus->poll(), forwards raw message buffer to inboundQueue.
// ─────────────────────────────────────────────────────────────────────────────
void receiverTaskWroom(void* params) {
    ReceiverParamsWroom* p = static_cast<ReceiverParamsWroom*>(params);
    char buf[256];
    while (true) {
        if (p->bus->poll(buf, sizeof(buf))) {
            if (xQueueSend(p->queue, buf, portMAX_DELAY) != pdTRUE) {
                // Queue full — message dropped. No Serial here (ISR-adjacent hot path).
                // Monitor queue depth via uxQueueSpacesAvailable() if drops occur.
            }
        }
    }
}
