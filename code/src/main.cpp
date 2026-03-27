#include <Arduino.h>
#include <Wire.h>

// =============================================
// CHANGE THESE TWO LINES TO SWAP GPIO PAIRS
// Try 1: GPIO3=SDA, GPIO10=SCL  (recommended first)
// Try 2: GPIO8=SDA, GPIO9=SCL
// Try 3: GPIO0=SDA, GPIO1=SCL
// =============================================
#define SDA_PIN 3
#define SCL_PIN 10

void scanBus(TwoWire &bus, const char *busName) {
  Serial.printf("\n========== Scanning: %s (SDA=%d, SCL=%d) ==========\n",
                busName, SDA_PIN, SCL_PIN);
  
  int foundCount = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    bus.beginTransmission(addr);
    uint8_t result = bus.endTransmission();

    if (result == 0) {
      Serial.printf("  [FOUND]   Address 0x%02X (%d)\n", addr, addr);
      foundCount++;
    } else {
      // result meanings:
      // 1 = data too long
      // 2 = NACK on address
      // 3 = NACK on data
      // 4 = other error
      // 5 = timeout
      Serial.printf("  [nothing] Address 0x%02X - code %d\n", addr, result);
    }

    delay(10); // small delay between probes
  }

  Serial.printf("\n  --> Scan complete. Found %d device(s).\n", foundCount);
}

void setup() {
  Serial.begin(115200);
  delay(2000); // give serial monitor time to connect

  Serial.println("\n\n====================================");
  Serial.println("    ESP32-C3 I2C Scanner Starting");
  Serial.printf("    SDA=GPIO%d  SCL=GPIO%d\n", SDA_PIN, SCL_PIN);
  Serial.println("====================================");

  // --- Method 1: Standard Wire with explicit pins ---
  Serial.println("\n[Method 1] Wire.begin() with explicit pins");
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000); // 100kHz - slow and safe
  scanBus(Wire, "Wire");
  Wire.end();
  delay(500);

  // --- Method 2: TwoWire instance ---
  Serial.println("\n[Method 2] TwoWire instance on bus 0");
  TwoWire I2C_B0 = TwoWire(0);
  I2C_B0.begin(SDA_PIN, SCL_PIN);
  I2C_B0.setClock(100000);
  scanBus(I2C_B0, "TwoWire(0)");
  I2C_B0.end();
  delay(500);

  // --- Method 3: Very slow clock ---
  Serial.println("\n[Method 3] Wire with very slow clock (50kHz)");
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(50000);
  scanBus(Wire, "Wire@50kHz");
  Wire.end();
  delay(500);

  Serial.println("\n====================================");
  Serial.println("    All scan methods complete.");
  Serial.println("====================================");
}

void loop() {
  // Heartbeat so you know the MCU is alive and loop() is running
  Serial.println("[HEARTBEAT] Scanner finished. MCU is alive.");
  delay(3000);
}