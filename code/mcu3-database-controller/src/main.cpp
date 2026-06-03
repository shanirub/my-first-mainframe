// ─────────────────────────────────────────────────────────────────────────────
// MCU #3 — Database Controller
// Phase 3.1 — i2c_new_slave_device() init, slave at 0x0A on GPIO8/9
//
// DoD:
//   3.1 No "GPIO 8/9 is not usable" warning in log
//       No WDT reset
//       "I2C slave init OK — addr 0x0A on GPIO8/9" in log
//       "on_receive callback registered" in log
//
// UART handshake + echo (Phase 1) commented out — preserved for Phase 5
// reintegration. Do not delete.
//
// Platform: espressif32 @ 6.10.0 (IDF 5.4), framework = espidf
// Board:    ESP32-WROOM-32 DevKit (38-pin, CP2102)
// ─────────────────────────────────────────────────────────────────────────────

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "oled_display_wroom.h"
#include "shared_bus_wroom.h"
#include "shared_state.h"
#include "config.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "MCU3";

// ── SharedState + mutex — definitions ────────────────────────────────────────
SharedState       gSharedState  = {};
SemaphoreHandle_t gDisplayMutex = nullptr;

// ── Shared bus ────────────────────────────────────────────────────────────────
static SharedBusWroom sharedBus;

// ═════════════════════════════════════════════════════════════════════════════
// UART — Phase 1 (commented out, preserved for Phase 5 reintegration)
// ═════════════════════════════════════════════════════════════════════════════

// #define UART_PORT            UART_NUM_1
// #define UART_BAUD            115200
// #define UART_RX_BUF          256
// #define UART_ECHO_TIMEOUT_MS 2000
//
// static void uart_init(void)
// {
//     const uart_config_t cfg = {
//         .baud_rate  = UART_BAUD,
//         .data_bits  = UART_DATA_8_BITS,
//         .parity     = UART_PARITY_DISABLE,
//         .stop_bits  = UART_STOP_BITS_1,
//         .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
//         .source_clk = UART_SCLK_DEFAULT,
//     };
//     ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
//     ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
//                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
//     ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_RX_BUF, 0, 0, NULL, 0));
//     ESP_LOGI(TAG, "UART1 init OK — TX GPIO%d RX GPIO%d @ %d baud",
//              UART_TX_PIN, UART_RX_PIN, UART_BAUD);
// }
//
// static bool uart_read_line(char* buf, int buf_len, uint32_t timeout_ms)
// {
//     int pos = 0;
//     uint32_t remaining_ms = timeout_ms;
//     while (remaining_ms > 0) {
//         TickType_t t0 = xTaskGetTickCount();
//         uint8_t c;
//         int n = uart_read_bytes(UART_PORT, &c, 1, pdMS_TO_TICKS(remaining_ms));
//         TickType_t elapsed_ms = (xTaskGetTickCount() - t0) * portTICK_PERIOD_MS;
//         remaining_ms = (elapsed_ms >= remaining_ms) ? 0 : remaining_ms - elapsed_ms;
//         if (n == 1) {
//             if (c == '\n') { buf[pos] = '\0'; return true; }
//             if (c != '\r' && pos < buf_len - 1) buf[pos++] = (char)c;
//         }
//     }
//     buf[pos] = '\0';
//     return false;
// }
//
// static void uart_handshake(void)
// {
//     ESP_LOGI(TAG, "UART handshake — waiting for PING from RPi...");
//     char buf[16];
//     while (true) {
//         if (uart_read_line(buf, sizeof(buf), 2000)) {
//             if (strcmp(buf, "PING") == 0) {
//                 const char* pong = "PONG\n";
//                 uart_write_bytes(UART_PORT, pong, strlen(pong));
//                 ESP_LOGI(TAG, "UART handshake OK — received PING, sent PONG");
//                 return;
//             } else {
//                 ESP_LOGW(TAG, "UART handshake — ignoring unexpected: \"%s\"", buf);
//             }
//         } else {
//             ESP_LOGI(TAG, "UART handshake — still waiting...");
//         }
//     }
// }
//
// static void uart_echo_test(void)
// {
//     const char* msg = "hello from mcu3";
//     char tx[32];
//     snprintf(tx, sizeof(tx), "%s\n", msg);
//     ESP_LOGI(TAG, "UART echo test — sending: \"%s\"", msg);
//     uart_write_bytes(UART_PORT, tx, strlen(tx));
//     char rx[32];
//     if (uart_read_line(rx, sizeof(rx), UART_ECHO_TIMEOUT_MS)) {
//         if (strcmp(rx, msg) == 0) {
//             ESP_LOGI(TAG, "UART echo OK — received: \"%s\"", rx);
//         } else {
//             ESP_LOGW(TAG, "UART echo MISMATCH — expected: \"%s\" got: \"%s\"", msg, rx);
//         }
//     } else {
//         ESP_LOGW(TAG, "UART echo TIMEOUT — no response within %dms", UART_ECHO_TIMEOUT_MS);
//     }
// }

// ═════════════════════════════════════════════════════════════════════════════
// OLED task — Phase 2.2
// ═════════════════════════════════════════════════════════════════════════════

static void oled_task(void* pvParameters)
{
    OledDisplayWroom* oled = static_cast<OledDisplayWroom*>(pvParameters);
    char line3[24];
    char line4[24];

    ESP_LOGI(TAG, "OLED task started");

    while (true) {
        SharedState snap;
        xSemaphoreTake(gDisplayMutex, portMAX_DELAY);
        snap = gSharedState;
        xSemaphoreGive(gDisplayMutex);

        snprintf(line3, sizeof(line3), "R:%lu W:%lu",
                 (unsigned long)snap.readCount,
                 (unsigned long)snap.writeCount);

        const char* acct = (snap.lastAccount[0] != '\0') ? snap.lastAccount : "--------";
        snprintf(line4, sizeof(line4), "Last: %s", acct);

        oled->showStatus("DATABASE CTRL", "Addr: 0x0A", line3, line4);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// app_main
// ═════════════════════════════════════════════════════════════════════════════

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "IDF version : %s", esp_get_idf_version());
    ESP_LOGI(TAG, "I2C address : 0x%02X", I2C_ADDRESS);
    ESP_LOGI(TAG, "MCU3 boot OK");

    // ── Phase 1: UART ─────────────────────────────────────────────────────────
    // Commented out for Phase 3.1 — reintegrate in Phase 5.
    // uart_init();
    // uart_handshake();
    // uart_echo_test();

    // ── Phase 2.2: displayMutex + OLED ───────────────────────────────────────
    // gDisplayMutex = xSemaphoreCreateMutex();
    // if (gDisplayMutex == nullptr) {
    //     ESP_LOGE(TAG, "displayMutex creation FAILED");
    //     return;
    // }

    // static OledDisplayWroom oled(OLED_SDA_PIN, OLED_SCL_PIN);
    // if (!oled.begin()) {
    //     ESP_LOGE(TAG, "OLED init FAILED");
    // } else {
    //     ESP_LOGI(TAG, "OLED init OK");
    // }

    // BaseType_t ret = xTaskCreate(oled_task, "oled", STACK_SIZE_OLED, &oled, 1, nullptr);
    // if (ret != pdPASS) {
    //     ESP_LOGE(TAG, "OLED task creation FAILED");
    // } else {
    //     ESP_LOGI(TAG, "OLED task created — 500ms refresh, priority 1");
    // }
    ESP_LOGI(TAG, "OLED task SKIPPED for isolation test");

    // ── Phase 3.1: shared bus slave init ─────────────────────────────────────
    // DoD: no GPIO8/9 warning, no WDT, both log lines below appear:
    //   "I2C slave init OK — addr 0x0A on GPIO8/9"
    //   "on_receive callback registered"
    ESP_LOGI(TAG, "Phase 3.1 — calling sharedBus.init()");
    sharedBus.init(I2C_ADDRESS);
    ESP_LOGI(TAG, "Phase 3.1 — sharedBus.init() returned");

    ESP_LOGI(TAG, "Phase 3.1 — app_main complete, tasks running");
}