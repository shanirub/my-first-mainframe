#include <Arduino.h>
#include "config.h"
#include "oled_display.h"

// ─────────────────────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────────────────────
OledDisplay oled(OLED_SDA_PIN, OLED_SCL_PIN);

// ─────────────────────────────────────────────────────────────────────────────
// uartLoopbackTest — Serial2 round-trip write/read test.
// Wire GPIO18 (TX) → GPIO19 (RX) before running.
// ─────────────────────────────────────────────────────────────────────────────
void uartLoopbackTest() {
    Serial.println("[MCU3-UART] Starting UART loopback test...");

    // UART1 on GPIO18=TX, GPIO19=RX
    Serial2.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    delay(100);

    const char* testStr = "hello from mcu3\n";
    Serial2.print(testStr);

    // Wait up to 500ms for response
    unsigned long start = millis();
    String response = "";
    while (millis() - start < 500) {
        if (Serial2.available()) {
            char c = Serial2.read();
            response += c;
            if (c == '\n') break;
        }
    }

    if (response == String(testStr)) {
        Serial.println("[MCU3-UART] PASS — round-trip verified");
        Serial.printf("[MCU3-UART] Received: \"%s\"\n", response.c_str());
        oled.showStatus("DATABASE CTRL", "UART: PASS", "loopback OK", "");
    } else {
        Serial.printf("[MCU3-UART] FAIL — sent: \"%s\"\n", testStr);
        Serial.printf("[MCU3-UART] received: \"%s\"\n", response.c_str());
        oled.showStatus("DATABASE CTRL", "UART: FAIL", "see serial", "");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("[MCU3] Database Controller starting — UART loopback test");

    if (!oled.begin()) {
        Serial.println("[MCU3] OLED failed");
    }
    oled.showStatus("DATABASE CTRL", "UART test...", "", "");

    uartLoopbackTest();
}

void loop() {
    // nothing
}