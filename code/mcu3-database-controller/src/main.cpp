#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include "config.h"
#include "oled_display.h"

// ─────────────────────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────────────────────
OledDisplay oled(OLED_SDA_PIN, OLED_SCL_PIN);
SdExFat     sd;

// ─────────────────────────────────────────────────────────────────────────────
// sdTest — SD init + round-trip write/read test.
// ─────────────────────────────────────────────────────────────────────────────
void sdTest() {
    Serial.println("[MCU3-SD] Starting SD init test...");

    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, -1);

    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    delay(500);

    SdSpiConfig sdConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(1));
    
    if (!sd.begin(sdConfig)) {
        Serial.println("[MCU3-SD] FAIL — sd.begin() returned false");
        Serial.printf("[MCU3-SD] SdFat error code: 0x%02X, data: 0x%02X\n",
            sd.sdErrorCode(), sd.sdErrorData());
        Serial.printf("[MCU3-SD] Pins: MISO=GPIO%d MOSI=GPIO%d SCK=GPIO%d CS=GPIO%d\n",
            SD_MISO_PIN, SD_MOSI_PIN, SD_SCK_PIN, SD_CS_PIN);
        oled.showStatus("DATABASE CTRL", "SD: FAIL", "see serial", "");
        return;
    }
    Serial.println("[MCU3-SD] sd.begin() OK");

    switch (sd.card()->type()) {
        case SD_CARD_TYPE_SD1:  Serial.println("[MCU3-SD] Card type: SD1");  break;
        case SD_CARD_TYPE_SD2:  Serial.println("[MCU3-SD] Card type: SD2");  break;
        case SD_CARD_TYPE_SDHC: Serial.println("[MCU3-SD] Card type: SDHC"); break;
        default:                Serial.println("[MCU3-SD] Card type: unknown"); break;
    }
    Serial.printf("[MCU3-SD] Card size: %lu MB\n",
        sd.card()->sectorCount() / 2048);

    // Write
    ExFile f;
    if (!f.open("test.txt", O_WRONLY | O_CREAT | O_TRUNC)) {
        Serial.println("[MCU3-SD] FAIL — could not open test.txt for write");
        oled.showStatus("DATABASE CTRL", "SD: FAIL", "write failed", "");
        return;
    }
    f.print("sd_round_trip_ok");
    f.close();
    Serial.println("[MCU3-SD] write OK");

    // Read back
    if (!f.open("test.txt", O_RDONLY)) {
        Serial.println("[MCU3-SD] FAIL — could not open test.txt for read");
        oled.showStatus("DATABASE CTRL", "SD: FAIL", "read failed", "");
        return;
    }
    char readBuf[32] = {0};
    int  n = f.read(readBuf, sizeof(readBuf) - 1);
    readBuf[n] = '\0';
    f.close();
    Serial.printf("[MCU3-SD] read back: \"%s\"\n", readBuf);

    if (strcmp(readBuf, "sd_round_trip_ok") == 0) {
        Serial.println("[MCU3-SD] PASS — round-trip verified");
        oled.showStatus("DATABASE CTRL", "SD: PASS", "round-trip OK", "");
    } else {
        Serial.println("[MCU3-SD] FAIL — read-back does not match written data");
        oled.showStatus("DATABASE CTRL", "SD: FAIL", "mismatch", "");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("[MCU3] Database Controller starting — SD test");

    if (!oled.begin()) {
        Serial.println("[MCU3] OLED failed");
    }
    oled.showStatus("DATABASE CTRL", "SD test...", "", "");

    sdTest();
}

void loop() {
    // nothing — no FreeRTOS tasks, no vTaskDelete needed
}