#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

#define SDA_PIN 3
#define SCL_PIN 10

// This is the key: TwoWire instance, NOT Wire
TwoWire I2C_OLED = TwoWire(0);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_OLED, -1);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Starting display...");

  // Initialize bus explicitly — same method that found 0x3C in scanner
  I2C_OLED.begin(SDA_PIN, SCL_PIN);
  I2C_OLED.setClock(100000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("FAIL: display.begin() returned false");
    while (true) {
      Serial.println("[HEARTBEAT] display init failed, stuck here");
      delay(3000);
    }
  }

  Serial.println("PASS: display.begin() succeeded");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Hello!");
  display.println("OLED works.");
  display.println("GPIO3=SDA");
  display.println("GPIO10=SCL");
  display.display();

  Serial.println("PASS: display.display() called");
}

void loop() {
  Serial.println("[HEARTBEAT] running");
  delay(3000);
}