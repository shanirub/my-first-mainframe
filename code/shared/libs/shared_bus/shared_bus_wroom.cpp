// ─────────────────────────────────────────────────────────────────────────────
// SharedBusWroom — implementation
// IDF 5.4.0, i2c_slave.h v2, I2C_NUM_0, GPIO8/9
// ─────────────────────────────────────────────────────────────────────────────
#include "shared_bus_wroom.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "SharedBusWroom";

// Static member definition — one instance only, set in init().
SharedBusWroom* SharedBusWroom::_instance = nullptr;

// ── Constructor ───────────────────────────────────────────────────────────────
SharedBusWroom::SharedBusWroom()
    : _handle(nullptr)
    , _address(0)
    , _rxLen(0)
    , _rxSemaphore(nullptr)
{
    memset(_rxBuf, 0, sizeof(_rxBuf));
}

// ── init() ────────────────────────────────────────────────────────────────────
void SharedBusWroom::init(uint8_t address)
{
    _address  = address;
    _instance = this;

    // Binary semaphore (not mutex): ISR gives, poll() takes.
    // Created before i2c_new_slave_device() so ISR can never fire before
    // the semaphore exists.
    _rxSemaphore = xSemaphoreCreateBinary();
    if (_rxSemaphore == nullptr) {
        ESP_LOGE(TAG, "rxSemaphore creation failed — out of memory");
        return;
    }
    ESP_LOGI(TAG, "rxSemaphore created OK");

    // i2c_slave_config_t fields (verified from i2c_slave.h under v2 guard):
    //   i2c_port         — I2C_NUM_0: shared inter-MCU bus
    //   sda_io_num       — GPIO8
    //   scl_io_num       — GPIO9
    //   clk_source       — I2C_CLK_SRC_DEFAULT: let IDF pick
    //   send_buf_depth   — TX ringbuffer depth (bytes). 256 gives comfortable
    //                      headroom for multi-message bursts before master reads.
    //   receive_buf_depth — RX software buffer depth (bytes). 256 matches _rxBuf.
    //   slave_addr       — our I2C address (0x0A)
    //   addr_bit_len     — I2C_ADDR_BIT_LEN_7: standard 7-bit addressing
    const i2c_slave_config_t cfg = {
        .i2c_port          = I2C_NUM_0,
        .sda_io_num        = GPIO_NUM_8,
        .scl_io_num        = GPIO_NUM_9,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .send_buf_depth    = 256,
        .receive_buf_depth = 256,
        .slave_addr        = address,
        .addr_bit_len      = I2C_ADDR_BIT_LEN_7,
        .intr_priority     = 0,   // default interrupt priority
        .flags             = {},  // no special flags
    };
    ESP_LOGI(TAG, "GPIO8 level: %d", gpio_get_level(GPIO_NUM_8));
    ESP_LOGI(TAG, "GPIO9 level: %d", gpio_get_level(GPIO_NUM_9));
    ESP_LOGI(TAG, "free heap before slave init: %lu", (unsigned long)esp_get_free_heap_size());

    esp_err_t err = i2c_new_slave_device(&cfg, &_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_slave_device failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "I2C slave init OK — addr 0x%02X on GPIO8/9", address);

    // Register on_receive callback.
    // i2c_slave_event_callbacks_t has two fields under v2:
    //   on_request — master requests data (TX direction)
    //   on_receive — master writes data to us (RX direction)
    // We only need on_receive for Phase 3. on_request left null.
    const i2c_slave_event_callbacks_t cbs = {
        .on_request = nullptr,
        .on_receive = on_receive,
    };

    // err = i2c_slave_register_event_callbacks(_handle, &cbs, this);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "register_event_callbacks failed: %s", esp_err_to_name(err));
    //     return;
    // }
    // ESP_LOGI(TAG, "on_receive callback registered");
    ESP_LOGI(TAG, "on_receive callback SKIPPED for isolation test");
}

// ── on_receive — ISR callback ─────────────────────────────────────────────────
// Called by IDF when master completes a write to our slave address.
// Runs in ISR context — no blocking calls, no heap allocation.
//
// evt_data->buffer: pointer into IDF's internal ringbuffer — valid only during
//   this callback. Must memcpy before returning.
// evt_data->length: byte count received (v2 only — confirmed in i2c_types.h).
//
// Returns bool: true if a higher-priority task was woken.
// IDF calls portYIELD_FROM_ISR() with this value after callback returns.
bool IRAM_ATTR SharedBusWroom::on_receive(
    i2c_slave_dev_handle_t handle,
    const i2c_slave_rx_done_event_data_t* evt_data,
    void* arg)
{
    if (_instance == nullptr || evt_data == nullptr || evt_data->buffer == nullptr) {
        return false;
    }

    // Clamp to _rxBuf capacity, leaving room for null terminator.
    uint32_t len = evt_data->length;
    if (len > sizeof(_instance->_rxBuf) - 1) {
        len = sizeof(_instance->_rxBuf) - 1;
    }

    // Copy out of IDF ringbuffer before returning — buffer pointer is only
    // valid during this callback.
    memcpy(_instance->_rxBuf, evt_data->buffer, len);
    _instance->_rxBuf[len] = '\0';
    _instance->_rxLen = len;

    // Signal poll() that a message is ready.
    BaseType_t xTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(_instance->_rxSemaphore, &xTaskWoken);

    // Return whether a higher-priority task was woken.
    // IDF will call portYIELD_FROM_ISR() with this value.
    return xTaskWoken == pdTRUE;
}

// ── poll() ────────────────────────────────────────────────────────────────────
// Blocks Receiver task until on_receive fires and gives _rxSemaphore.
// Copies _rxBuf into caller's buf. Safe: semaphore take establishes
// happens-before with the ISR's semaphore give — _rxBuf is fully written
// before we read it.
bool SharedBusWroom::poll(char* buf, int bufLen)
{
    if (xSemaphoreTake(_rxSemaphore, portMAX_DELAY) != pdTRUE) {
        // portMAX_DELAY should never time out — defensive only.
        ESP_LOGW(TAG, "poll() semaphore take failed");
        return false;
    }

    int copyLen = (_rxLen < (uint32_t)(bufLen - 1)) ? (int)_rxLen : bufLen - 1;
    memcpy(buf, _rxBuf, copyLen);
    buf[copyLen] = '\0';

    return true;
}

// ── send() ────────────────────────────────────────────────────────────────────
// Pre-loads TX ringbuffer so master can read on demand.
// i2c_slave_write() signature (verified):
//   (handle, data, len, write_len_out, timeout_ms)
// timeout_ms = 0: non-blocking — returns immediately if ringbuffer has space.
// write_len: actual bytes accepted into ringbuffer (may be less than requested
// if buffer is full — logged as a warning).
BusError SharedBusWroom::send(uint8_t targetAddress, const char* message)
{
    (void)targetAddress;  // slave has no addressing control — ignored

    uint32_t msgLen = (uint32_t)strlen(message);
    uint32_t written = 0;

    esp_err_t err = i2c_slave_write(
        _handle,
        reinterpret_cast<const uint8_t*>(message),
        msgLen,
        &written,
        0   // timeout_ms = 0: non-blocking
    );

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "i2c_slave_write failed: %s", esp_err_to_name(err));
        return BusError::BUS_FAULT;
    }
    if (written < msgLen) {
        ESP_LOGW(TAG, "i2c_slave_write partial: %lu/%lu bytes", (unsigned long)written, (unsigned long)msgLen);
        return BusError::BUS_FAULT;
    }

    ESP_LOGD(TAG, "send() loaded %lu bytes into TX ringbuffer", (unsigned long)written);
    return BusError::OK;
}

// ── receiverTaskWroom() ───────────────────────────────────────────────────────
// Blocks on poll(), forwards each message to inboundQueue for Logic task.
// Queue send uses timeout 0 — if queue is full, message is dropped and logged.
// A full queue means Logic task is not keeping up; tune queue depth or
// Logic task priority if this occurs in practice.
void receiverTaskWroom(void* params)
{
    auto* p = static_cast<ReceiverParamsWroom*>(params);
    char buf[256];

    ESP_LOGI(TAG, "Receiver task started");

    while (true) {
        if (p->bus->poll(buf, sizeof(buf))) {
            ESP_LOGD(TAG, "Receiver: message received: \"%s\"", buf);

            // Copy buf into a fixed-size queue message.
            // Queue holds char arrays — Logic task reads and parses.
            char qMsg[256];
            strncpy(qMsg, buf, sizeof(qMsg) - 1);
            qMsg[sizeof(qMsg) - 1] = '\0';

            if (xQueueSend(p->inboundQueue, qMsg, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Receiver: inboundQueue full — message dropped");
            }
        }
    }
}