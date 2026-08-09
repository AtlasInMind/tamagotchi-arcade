// Basic board test for ESP32-based LilyGO TTGO / clone (1.14" LCD, 16MB flash)
// No display library needed yet - just confirms flashing + serial + onboard LED work.

const int LED_PIN = 2; // Most ESP32 dev boards use GPIO2 for the onboard LED.
                        // If nothing blinks, the LED may be on a different pin (or absent) - that's fine, Serial output still confirms the upload worked.

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);

  Serial.println();
  Serial.println("=== TTGO board test starting ===");
  Serial.printf("Chip model:      %s\n", ESP.getChipModel());
  Serial.printf("Chip revision:   %d\n", ESP.getChipRevision());
  Serial.printf("CPU freq:        %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash size:      %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("Free heap:       %d bytes\n", ESP.getFreeHeap());
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ON");
  delay(1000);

  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");
  delay(1000);
}
