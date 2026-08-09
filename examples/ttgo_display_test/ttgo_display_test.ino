// Display test for 1.14" ST7789 LCD (135x240) on ESP32 TTGO T-Display / clone.
// Uses TFT_eSPI, configured via User_Setups/Setup25_TTGO_T_Display.h.
// Cycles background colors, then draws text, to confirm the panel and pins are correct.

#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void colorFlash(uint16_t color, const char* name) {
  tft.fillScreen(color);
  tft.setTextColor(TFT_WHITE, color);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(name, tft.width() / 2, tft.height() / 2, 4);
  delay(800);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("TFT display test starting...");

  tft.init();
  tft.setRotation(1); // landscape

  colorFlash(TFT_RED, "RED");
  colorFlash(TFT_GREEN, "GREEN");
  colorFlash(TFT_BLUE, "BLUE");
  colorFlash(TFT_BLACK, "BLACK");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Hello, TTGO!", tft.width() / 2, tft.height() / 2 - 10, 4);

  Serial.println("Display test complete.");
}

void loop() {
  // Draw a moving pixel/line as a simple heartbeat so we know it's still alive.
  static int x = 0;
  tft.drawFastVLine(x, 0, 8, TFT_BLACK); // erase previous
  x = (x + 1) % tft.width();
  tft.drawFastVLine(x, 0, 8, TFT_YELLOW);
  delay(20);
}
