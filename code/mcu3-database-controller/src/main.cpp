#include <Arduino.h>
#include "config.h"
#include "oled_display.h"

// ─────────────────────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────────────────────
OledDisplay oled(OLED_SDA_PIN, OLED_SCL_PIN);

// ─────────────────────────────────────────────────────────────────────────────
// readLine — reads from Serial2 until '\n' or timeout (ms).
// Returns true and fills buf if a complete line arrived.
// Returns false if timeout elapsed before '\n'.
// ─────────────────────────────────────────────────────────────────────────────
bool readLine(char* buf, int bufLen, unsigned long timeoutMs) {
    unsigned long start = millis();
    int pos = 0;
    while (millis() - start < timeoutMs) {
        if (Serial2.available()) {
            char c = Serial2.read();
            Serial.printf("[MCU3-UART] byte: 0x%02X '%c'\n", c, c);
            if (c == '\n') {
                buf[pos] = '\0';
                return true;
            }
            if (pos < bufLen - 1) buf[pos++] = c;
        }
    }
    buf[pos] = '\0';
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// uartHandshake — waits for PING from RPi, responds PONG.
// RPi initiates because it boots slower — MCU #3 just listens.
// Blocks until handshake completes. No timeout — RPi must come up eventually.
// ─────────────────────────────────────────────────────────────────────────────
void uartHandshake() {
    Serial.println("[MCU3-UART] Waiting for PING from RPi...");
    oled.showStatus("DATABASE CTRL", "UART...", "waiting PING", "");

    char buf[16];
    while (true) {
        if (readLine(buf, sizeof(buf), 2000)) {
            if (strcmp(buf, "PING") == 0) {
                Serial2.print("PONG\n");
                Serial.println("[MCU3-UART] Received PING — sent PONG");
                return;
            } else {
                // Unexpected line during handshake — ignore and keep waiting
                Serial.printf("[MCU3-UART] Ignoring unexpected: \"%s\"\n", buf);
            }
        }
        // No line within 2000ms — keep waiting, print a dot to show we're alive
        Serial.print(".");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// uartEchoTest — sends a message to RPi, waits for echo back.
// Must be called after uartHandshake() completes.
// ─────────────────────────────────────────────────────────────────────────────
void uartEchoTest() {
    Serial.println("[MCU3-UART] Starting echo test...");

    const char* testStr = "hello from mcu3";
    Serial.printf("[MCU3-UART] Sending: \"%s\"\n", testStr);
    Serial2.printf("%s\n", testStr);

    char buf[64];
    if (readLine(buf, sizeof(buf), 2000)) {
        if (strcmp(buf, testStr) == 0) {
            Serial.println("[MCU3-UART] PASS — echo verified");
            oled.showStatus("DATABASE CTRL", "UART: PASS", "echo OK", "");
        } else {
            Serial.printf("[MCU3-UART] FAIL — expected: \"%s\"\n", testStr);
            Serial.printf("[MCU3-UART] received:  \"%s\"\n", buf);
            oled.showStatus("DATABASE CTRL", "UART: FAIL", "mismatch", "");
        }
    } else {
        Serial.println("[MCU3-UART] FAIL — timeout, no response from RPi");
        oled.showStatus("DATABASE CTRL", "UART: FAIL", "timeout", "");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("[MCU3] Database Controller starting — UART echo test");

    if (!oled.begin()) {
        Serial.println("[MCU3] OLED failed");
    }

    Serial2.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

    // Flush any boot noise before entering handshake
    delay(500);
    while (Serial2.available()) Serial2.read();

    uartHandshake();
    uartEchoTest();
}

void loop() {
    // nothing
}