#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "config.h"
#include "oled_display.h"

/* OledDisplay oled(OLED_SDA_PIN, OLED_SCL_PIN);
 */
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("[MCU3-SD] SD card init test starting");

/*     if (!oled.begin()) {
        Serial.println("[MCU3-SD] OLED failed");
    }
    oled.showStatus("DATABASE CTRL", "SD test...", "", ""); */

    // Explicitly configure SPI pins before SD.begin().
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    // SD.begin() takes only the CS pin — SPI bus already configured above.
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("[MCU3-SD] FAIL: SD init failed — check wiring");
        Serial.printf("[MCU3-SD] Check: MISO=GPIO%d, MOSI=GPIO%d, SCK=GPIO%d, CS=GPIO%d\n",
            SD_MISO_PIN, SD_MOSI_PIN, SD_SCK_PIN, SD_CS_PIN);
        Serial.printf("[MCU3-SD] Card type: %d\n", SD.cardType());
/*         oled.showStatus("DATABASE CTRL", "SD: FAIL", "Check wiring", "");
 */        return;
    }

    Serial.println("[MCU3-SD] PASS: SD card initialized");

    // Print card size in MB.
    uint64_t cardSizeMB = SD.cardSize() / (1024 * 1024);
    Serial.printf("[MCU3-SD] Card size: %llu MB\n", cardSizeMB);

    // Try writing a test file.
    File testFile = SD.open("/test.txt", FILE_WRITE);
    if (!testFile) {
        Serial.println("[MCU3-SD] FAIL: could not create test.txt");
/*         oled.showStatus("DATABASE CTRL", "SD: FAIL", "Write failed", "");
 */        return;
    }
    testFile.println("MCU3 SD test OK");
    testFile.close();
    Serial.println("[MCU3-SD] PASS: test.txt written successfully");

    // Try reading it back.
    File readFile = SD.open("/test.txt");
    if (!readFile) {
        Serial.println("[MCU3-SD] FAIL: could not read test.txt back");
/*         oled.showStatus("DATABASE CTRL", "SD: FAIL", "Read failed", "");
 */        return;
    }
    char buf[32];
    memset(buf, 0, sizeof(buf));
    int n = 0;
    while (readFile.available() && n < (int)sizeof(buf) - 1) {
        buf[n++] = readFile.read();
    }
    readFile.close();
    Serial.printf("[MCU3-SD] PASS: read back: %s\n", buf);

    Serial.println("[MCU3-SD] ALL PASS: SD card wiring confirmed good");
/*     oled.showStatus("DATABASE CTRL", "SD: PASS", "Wiring OK", "");
 */}

void loop() {
    vTaskDelete(NULL);
}