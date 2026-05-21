// ─────────────────────────────────────────────────────────────────────────────
// MCU #3 — Database Controller
// Phase 1 — UART to RPi: handshake + echo test
//
// DoD:
//   1.1 RPi running, handshake completes — "UART handshake OK" in log
//   1.2 Echo test passes — "UART echo OK" in log
//   1.2 RPi down — timeout fires within 2000ms, "UART echo TIMEOUT" in log
//
// Platform: espressif32 @ 6.10.0 (IDF 5.4), framework = espidf
// Board:    ESP32-WROOM-32 DevKit (38-pin, CP2102)
// ─────────────────────────────────────────────────────────────────────────────

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "config.h"
#include <cstring>

static const char *TAG = "MCU3";

// ── UART configuration ───────────────────────────────────────────────────────
// UART_NUM_1 = UART1 hardware peripheral — separate from UART0 (debug/USB).
// RX buffer: 256 bytes. TX buffer: 0 (blocking transmit is fine at this stage).
// No event queue needed yet — added in Phase 4 when the UART task is created.
#define UART_PORT       UART_NUM_1
#define UART_BAUD       115200
#define UART_RX_BUF     256
#define UART_ECHO_TIMEOUT_MS  2000

static void uart_init(void)
{
    const uart_config_t cfg = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // TX buffer 0 = synchronous writes (blocks until bytes in HW FIFO).
    // Acceptable here — no concurrent tasks yet.
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_RX_BUF, 0, 0, NULL, 0));
    ESP_LOGI(TAG, "UART1 init OK — TX GPIO%d RX GPIO%d @ %d baud",
             UART_TX_PIN, UART_RX_PIN, UART_BAUD);
}

// ── Read a newline-terminated line from UART1 ────────────────────────────────
// Reads byte-by-byte into buf (max bufLen-1 chars + '\0').
// Returns true if '\n' received before timeout_ms elapsed.
// Returns false on timeout — buf contains whatever arrived.
// Note: uart_read_bytes() timeout is in RTOS ticks; pdMS_TO_TICKS converts.
static bool uart_read_line(char *buf, int buf_len, uint32_t timeout_ms)
{
    int pos = 0;
    uint32_t remaining_ms = timeout_ms;

    while (remaining_ms > 0) {
        TickType_t t0 = xTaskGetTickCount();
        uint8_t c;
        int n = uart_read_bytes(UART_PORT, &c, 1, pdMS_TO_TICKS(remaining_ms));
        ESP_LOGI(TAG, "uart_read_bytes returned %d", n);
        TickType_t elapsed_ms = (xTaskGetTickCount() - t0) * portTICK_PERIOD_MS;
        remaining_ms = (elapsed_ms >= remaining_ms) ? 0 : remaining_ms - elapsed_ms;

        if (n == 1) {
            if (c == '\n') {
                buf[pos] = '\0';
                return true;
            }
            if (c != '\r' && pos < buf_len - 1) {
                buf[pos++] = (char)c;
            }
        }
    }
    buf[pos] = '\0';
    return false;
}

// ── Handshake — wait for PING, respond PONG ──────────────────────────────────
// RPi initiates because it boots slower than MCU #3.
// Blocks indefinitely — RPi must come up eventually.
// Logs a heartbeat every ~2s so the developer knows MCU is alive.
static void uart_handshake(void)
{
    ESP_LOGI(TAG, "UART handshake — waiting for PING from RPi...");
    char buf[16];

    while (true) {
        if (uart_read_line(buf, sizeof(buf), 2000)) {
            if (strcmp(buf, "PING") == 0) {
                const char *pong = "PONG\n";
                uart_write_bytes(UART_PORT, pong, strlen(pong));
                ESP_LOGI(TAG, "UART handshake OK — received PING, sent PONG");
                return;
            } else {
                ESP_LOGW(TAG, "UART handshake — ignoring unexpected: \"%s\"", buf);
            }
        } else {
            // Timeout — no line in 2000ms. RPi not up yet, keep waiting.
            ESP_LOGI(TAG, "UART handshake — still waiting...");
        }
    }
}

// ── Echo test — send a string, expect it back ────────────────────────────────
static void uart_echo_test(void)
{
    const char *msg     = "hello from mcu3";
    char        tx[32];
    snprintf(tx, sizeof(tx), "%s\n", msg);

    ESP_LOGI(TAG, "UART echo test — sending: \"%s\"", msg);
    uart_write_bytes(UART_PORT, tx, strlen(tx));

    char rx[32];
    if (uart_read_line(rx, sizeof(rx), UART_ECHO_TIMEOUT_MS)) {
        if (strcmp(rx, msg) == 0) {
            ESP_LOGI(TAG, "UART echo OK — received: \"%s\"", rx);
        } else {
            ESP_LOGW(TAG, "UART echo MISMATCH — expected: \"%s\" got: \"%s\"", msg, rx);
        }
    } else {
        ESP_LOGW(TAG, "UART echo TIMEOUT — no response within %dms", UART_ECHO_TIMEOUT_MS);
    }
}

// ── app_main ─────────────────────────────────────────────────────────────────
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "IDF version : %s", esp_get_idf_version());
    ESP_LOGI(TAG, "I2C address : 0x%02X", I2C_ADDRESS);
    ESP_LOGI(TAG, "MCU3 boot OK");

    uart_init();
    ESP_LOGI(TAG, "UART pins — TX:%d RX:%d", UART_TX_PIN, UART_RX_PIN);
    uart_handshake();
    uart_echo_test();

    ESP_LOGI(TAG, "Phase 1 complete — entering idle loop");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}