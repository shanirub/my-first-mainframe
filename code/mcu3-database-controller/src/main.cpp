// ─────────────────────────────────────────────────────────────────────────────
// MCU #3 — Database Controller
// Phase 2.2 — OLED task: 500ms refresh, SharedState under displayMutex
//
// DoD:
//   2.2 OLED task running at priority 1, refreshes every 500ms.
//       Displays live SharedState (zeroed at boot — no Logic task yet).
//       60-second soak: no crash, no WDT, OLED continuously updating.
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
#include "shared_state.h"
#include "config.h"
#include <cstring>
#include <cstdio>

static const char *TAG = "MCU3";

// STACK_SIZE_OLED = 2048 — defined in shared_config.h (via config.h).
// Receiver/OLED tasks are shallow: no JsonDocument, no printf formatting on stack.
// Tune with uxTaskGetStackHighWaterMark() in Phase 6 if needed.

// ── SharedState + mutex — definitions ────────────────────────────────────────
// Declared extern in shared_state.h; defined here once.
// displayMutex created in app_main() before xTaskCreate() — guaranteed
// non-null when the OLED task first runs.
SharedState       gSharedState = {};   // zero-initialised at boot
SemaphoreHandle_t gDisplayMutex = nullptr;

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
            ESP_LOGI(TAG, "UART handshake — still waiting...");
        }
    }
}

// ── Echo test — send a string, expect it back ────────────────────────────────
static void uart_echo_test(void)
{
    const char *msg = "hello from mcu3";
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

// ── OLED task ─────────────────────────────────────────────────────────────────
// Runs at priority 1 (lowest application priority).
// Wakes every 500ms, takes displayMutex, snapshots SharedState, releases
// mutex, then renders to OLED. Mutex is NOT held during the I2C transfer —
// minimises contention time for Logic task (Phase 4).
//
// Layout:
//   DATABASE CTRL
//   Addr: 0x0A
//   R:0 W:0
//   Last: --------
static void oled_task(void *pvParameters)
{
    OledDisplayWroom *oled = static_cast<OledDisplayWroom *>(pvParameters);

    char line3[24];  // "R:XXXXXXXX W:XXXXXXXX"
    char line4[24];  // "Last: XXXXXXXX"

    ESP_LOGI(TAG, "OLED task started");

    while (true) {
        // ── Snapshot SharedState under mutex ─────────────────────────────────
        // Take with portMAX_DELAY: Logic task (Phase 4) holds mutex only for
        // a field update — contention window is microseconds, not a concern.
        SharedState snap;
        xSemaphoreTake(gDisplayMutex, portMAX_DELAY);
        snap = gSharedState;   // struct copy — safe, no pointers
        xSemaphoreGive(gDisplayMutex);

        // ── Format lines ─────────────────────────────────────────────────────
        snprintf(line3, sizeof(line3), "R:%lu W:%lu",
                 (unsigned long)snap.readCount,
                 (unsigned long)snap.writeCount);

        // Show dashes if no account seen yet
        const char *acct = (snap.lastAccount[0] != '\0') ? snap.lastAccount : "--------";
        snprintf(line4, sizeof(line4), "Last: %s", acct);

        // ── Render ───────────────────────────────────────────────────────────
        oled->showStatus("DATABASE CTRL", "Addr: 0x0A", line3, line4);

        ESP_LOGD(TAG, "OLED refresh — %s | %s", line3, line4);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
    // Unreachable — FreeRTOS tasks must not return. vTaskDelete not needed
    // here since the loop is infinite, but noted for completeness.
}

// ── app_main ─────────────────────────────────────────────────────────────────
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "IDF version : %s", esp_get_idf_version());
    ESP_LOGI(TAG, "I2C address : 0x%02X", I2C_ADDRESS);
    ESP_LOGI(TAG, "MCU3 boot OK");

    uart_init();
    uart_handshake();
    uart_echo_test();

    // ── Create displayMutex before any task that uses it ─────────────────────
    // Must happen in app_main(), not inside the task — tasks may run before
    // returning from xTaskCreate() on a multi-core MCU (ESP32 has two cores).
    gDisplayMutex = xSemaphoreCreateMutex();
    if (gDisplayMutex == nullptr) {
        ESP_LOGE(TAG, "displayMutex creation FAILED — heap exhausted?");
        return;  // cannot proceed safely
    }

    // ── OLED init ─────────────────────────────────────────────────────────────
    // Static allocation: persists for the lifetime of app_main's frame, which
    // never returns under espidf (app_main stack is kept alive by IDF).
    // Passed to oled_task via pvParameters — raw pointer, lifetime is safe.
    static OledDisplayWroom oled(OLED_SDA_PIN, OLED_SCL_PIN);
    if (!oled.begin()) {
        ESP_LOGE(TAG, "OLED init FAILED");
        // Non-fatal — OLED task will still run, display will be blank/undefined.
        // Do not return; the rest of the system should still boot.
    } else {
        ESP_LOGI(TAG, "OLED init OK");
    }

    // ── Create OLED task ──────────────────────────────────────────────────────
    // Priority 1: lowest application priority — display is best-effort.
    // Stack 4096 bytes: u8g2 SendBuffer does I2C on calling stack.
    BaseType_t ret = xTaskCreate(
        oled_task,
        "oled",
        STACK_SIZE_OLED,
        &oled,           // pvParameters: pointer to OledDisplayWroom instance
        1,               // priority
        nullptr          // task handle not needed
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "OLED task creation FAILED (ret=%d)", (int)ret);
    } else {
        ESP_LOGI(TAG, "OLED task created — 500ms refresh, priority 1");
    }

    // app_main() returns here — IDF keeps the scheduler running.
    // All work is now in tasks.
    ESP_LOGI(TAG, "Phase 2.2 — app_main complete, tasks running");
}