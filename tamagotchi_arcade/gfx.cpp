#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "gfx.h"

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
namespace Pal {
  const uint16_t BG           = 0x10C5; // approved deep navy
  const uint16_t SKY_A        = 0x4A4B; // muted dusk violet
  const uint16_t SKY_B        = 0x7226; // warm auburn shadow
  const uint16_t PANEL        = 0x2168; // blue-black raised panel
  const uint16_t PANEL_LIGHT  = 0x736E; // desaturated lavender edge
  const uint16_t PANEL_DARK   = 0x10A4;
  const uint16_t INK          = 0x10A4;
  const uint16_t PAPER        = 0xFF79; // warm cream from cat palette
  const uint16_t GOLD         = 0xF5ED;
  const uint16_t RED_ACCENT   = 0xF3AC;
  const uint16_t GREEN_ACCENT = 0x6C67;
  const int BODY_COUNT = 6;
  const uint16_t BODY[6] = {
    0x3667, // classic green
    0x2D9F, // ocean blue
    0xFB56, // bubblegum pink
    0x8817, // grape purple
    0xFC48, // sunset orange
    0xFEA0, // golden yellow
  };
}

uint16_t shade(uint16_t c, float factor) {
  if (factor > 1) factor = 1;
  if (factor < -1) factor = -1;
  int r = (c >> 11) & 0x1F;
  int g = (c >> 5) & 0x3F;
  int b = c & 0x1F;
  if (factor >= 0) {
    r += (int)((31 - r) * factor);
    g += (int)((63 - g) * factor);
    b += (int)((31 - b) * factor);
  } else {
    r += (int)(r * factor);
    g += (int)(g * factor);
    b += (int)(b * factor);
  }
  r = constrain(r, 0, 31);
  g = constrain(g, 0, 63);
  b = constrain(b, 0, 31);
  return (r << 11) | (g << 5) | b;
}

// ---------------------------------------------------------------------------
// Sprite maps
// ---------------------------------------------------------------------------
void drawSpriteMap(TFT_eSprite &dst, const char *const *rows, int rowCount,
                    int x, int y, int scale,
                    const SpriteLegend *legend, int legendCount) {
  for (int r = 0; r < rowCount; r++) {
    const char *row = rows[r];
    for (int c = 0; row[c] != '\0'; c++) {
      char ch = row[c];
      if (ch == ' ' || ch == '.') continue;
      uint16_t color = 0;
      bool found = false;
      for (int i = 0; i < legendCount; i++) {
        if (legend[i].key == ch) { color = legend[i].color; found = true; break; }
      }
      if (!found) continue;
      dst.fillRect(x + c * scale, y + r * scale, scale, scale, color);
    }
  }
}

int spriteMapWidth(const char *const *rows, int scale) {
  return (int)strlen(rows[0]) * scale;
}

int spriteMapHeight(int rowCount, int scale) {
  return rowCount * scale;
}

// ---------------------------------------------------------------------------
// Panels
// ---------------------------------------------------------------------------
static void panelCore(TFT_eSprite &dst, int x, int y, int w, int h, uint16_t topLeft, uint16_t bottomRight) {
  dst.fillRect(x + 2, y + 2, w, h, Pal::PANEL_DARK);       // drop shadow
  dst.fillRect(x, y, w, h, Pal::PANEL);                    // fill
  dst.drawFastHLine(x, y, w, topLeft);                      // raised top
  dst.drawFastVLine(x, y, h, topLeft);                      // raised left
  dst.drawFastHLine(x, y + h - 1, w, bottomRight);           // sunk bottom
  dst.drawFastVLine(x + w - 1, y, h, bottomRight);           // sunk right
  // Notched corners - a cheap stand-in for true rounding that keeps panels
  // from reading as harsh rectangles.
  dst.drawPixel(x, y, Pal::PANEL_DARK);
  dst.drawPixel(x + w - 1, y, Pal::PANEL_DARK);
  dst.drawPixel(x, y + h - 1, Pal::PANEL_DARK);
  dst.drawPixel(x + w - 1, y + h - 1, Pal::PANEL_DARK);
}

void drawPanel(TFT_eSprite &dst, int x, int y, int w, int h) {
  panelCore(dst, x, y, w, h, Pal::PANEL_LIGHT, Pal::PANEL_DARK);
}

// Same panel, but with a 1px accent trim on the top/left edge - used to mark
// premium-tier shop cells and other "this one is special" moments.
void drawPanelAccent(TFT_eSprite &dst, int x, int y, int w, int h, uint16_t accent) {
  panelCore(dst, x, y, w, h, accent, Pal::PANEL_DARK);
}

void drawPanelTitled(TFT_eSprite &dst, int x, int y, int w, int h, const char *title) {
  drawPanel(dst, x, y, w, h);
  drawPixelTextC(dst, title, x + w / 2, y + 4, 1, Pal::PAPER, Pal::INK);
  dst.drawFastHLine(x + 4, y + 14, w - 8, Pal::PANEL_DARK);
  dst.drawFastHLine(x + 4, y + 15, w - 8, Pal::PANEL_LIGHT);
}

// ---------------------------------------------------------------------------
// Pixel text (GLCD font 1)
// ---------------------------------------------------------------------------
void drawPixelText(TFT_eSprite &dst, const char *text, int x, int y, int size,
                    uint16_t color, uint16_t shadowColor) {
  dst.setTextFont(1);
  dst.setTextSize(size);
  dst.setTextDatum(TL_DATUM);
  if (shadowColor != color) {
    dst.setTextColor(shadowColor);
    dst.drawString(text, x + size, y + size);
  }
  dst.setTextColor(color);
  dst.drawString(text, x, y);
}

void drawPixelTextC(TFT_eSprite &dst, const char *text, int cx, int y, int size,
                     uint16_t color, uint16_t shadowColor) {
  int w = pixelTextWidth(text, size);
  drawPixelText(dst, text, cx - w / 2, y, size, color, shadowColor);
}

int pixelTextWidth(const char *text, int size) {
  return (int)strlen(text) * 6 * size;
}

// ---------------------------------------------------------------------------
// Dithered gradient
// ---------------------------------------------------------------------------
void drawDitherGradient(TFT_eSprite &dst, uint16_t colorA, uint16_t colorB, int y0, int y1) {
  static const uint8_t BAYER[4][4] = {
    { 0,  8,  2, 10 },
    { 12, 4, 14,  6 },
    { 3, 11,  1,  9 },
    { 15, 7, 13,  5 },
  };
  int h = y1 - y0;
  if (h <= 0) return;
  for (int y = y0; y < y1; y++) {
    float t = (float)(y - y0) / (float)h; // 0 at top -> 1 at bottom
    uint8_t threshold = (uint8_t)(t * 16.0f);
    for (int x = 0; x < SCREEN_W; x++) {
      uint8_t m = BAYER[y & 3][x & 3];
      dst.drawPixel(x, y, (m < threshold) ? colorB : colorA);
    }
  }
}

// ---------------------------------------------------------------------------
// Particles
// ---------------------------------------------------------------------------
struct Particle {
  bool active;
  float x, y, vx, vy;
  uint16_t color;
  float life;
};
static Particle particles[MAX_PARTICLES];

void particlesReset() {
  for (int i = 0; i < MAX_PARTICLES; i++) particles[i].active = false;
}

void particlesSpawnBurst(int x, int y, uint16_t color, int count) {
  for (int i = 0; i < MAX_PARTICLES && count > 0; i++) {
    if (particles[i].active) continue;
    float angle = random(0, 360) * (PI / 180.0f);
    float speed = 30 + random(0, 40);
    particles[i].active = true;
    particles[i].x = x;
    particles[i].y = y;
    particles[i].vx = cosf(angle) * speed;
    particles[i].vy = sinf(angle) * speed - 40; // initial upward kick
    particles[i].color = color;
    particles[i].life = 0.6f + (random(0, 40) / 100.0f);
    count--;
  }
}

void particlesTick(float dt) {
  const float GRAVITY = 160.0f;
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!particles[i].active) continue;
    particles[i].vy += GRAVITY * dt;
    particles[i].x += particles[i].vx * dt;
    particles[i].y += particles[i].vy * dt;
    particles[i].life -= dt;
    if (particles[i].life <= 0 || particles[i].y > SCREEN_H) particles[i].active = false;
  }
}

void particlesRender(TFT_eSprite &dst) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!particles[i].active) continue;
    dst.fillRect((int)particles[i].x, (int)particles[i].y, 2, 2, particles[i].color);
  }
}

// ---------------------------------------------------------------------------
// Toasts
// ---------------------------------------------------------------------------
enum ToastPhase { TOAST_IN, TOAST_HOLD, TOAST_OUT, TOAST_IDLE };
static ToastPhase toastPhase = TOAST_IDLE;
static char toastLine1[24];
static char toastLine2[24];
static uint16_t toastAccent;
static float toastY = -40;
static float toastTimer = 0;

void toastShow(const char *line1, const char *line2, uint16_t accent) {
  strncpy(toastLine1, line1, sizeof(toastLine1) - 1);
  toastLine1[sizeof(toastLine1) - 1] = '\0';
  strncpy(toastLine2, line2 ? line2 : "", sizeof(toastLine2) - 1);
  toastLine2[sizeof(toastLine2) - 1] = '\0';
  toastAccent = accent;
  toastY = -40;
  toastPhase = TOAST_IN;
  toastTimer = 2.2f;
}

void toastTick(float dt) {
  switch (toastPhase) {
    case TOAST_IDLE: return;
    case TOAST_IN:
      toastY += 220 * dt;
      if (toastY >= 8) { toastY = 8; toastPhase = TOAST_HOLD; }
      break;
    case TOAST_HOLD:
      toastTimer -= dt;
      if (toastTimer <= 0) toastPhase = TOAST_OUT;
      break;
    case TOAST_OUT:
      toastY -= 220 * dt;
      if (toastY < -40) toastPhase = TOAST_IDLE;
      break;
  }
}

void toastRender(TFT_eSprite &dst) {
  if (toastPhase == TOAST_IDLE) return;
  int w = 118, h = 32;
  int x = (SCREEN_W - w) / 2;
  int y = (int)toastY;
  drawPanel(dst, x, y, w, h);
  dst.drawRect(x - 1, y - 1, w + 2, h + 2, toastAccent);
  drawPixelTextC(dst, toastLine1, x + w / 2, y + 6, 1, Pal::PAPER, Pal::INK);
  if (toastLine2[0]) drawPixelTextC(dst, toastLine2, x + w / 2, y + 18, 1, toastAccent, Pal::INK);
}
