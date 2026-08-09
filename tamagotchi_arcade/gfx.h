#pragma once
#include <TFT_eSPI.h>

// Portrait, USB-at-bottom. If this reads upside-down on your particular
// board/cable, flip to 2 - everything else is orientation-agnostic.
#define SCREEN_ROTATION 0
#define SCREEN_W 135
#define SCREEN_H 240

// ---------------------------------------------------------------------------
// Palette
//
// GBA-era art gets its cohesion from a small, disciplined palette rather than
// from any one color choice. Instead of hand-authoring dozens of named tones,
// we keep a short list of BASE colors and derive shadow/highlight tones from
// them at draw time with shade() - a 3-tone ramp per material, same idea as
// the original hardware's palette-index shading, without needing to store
// every derived tone.
// ---------------------------------------------------------------------------
namespace Pal {
  extern const uint16_t BG;           // deep backdrop behind everything
  extern const uint16_t SKY_A;        // dithered background gradient, top
  extern const uint16_t SKY_B;        // dithered background gradient, bottom
  extern const uint16_t PANEL;        // menu panel fill
  extern const uint16_t PANEL_LIGHT;  // panel top/left border (raised look)
  extern const uint16_t PANEL_DARK;   // panel bottom/right border + drop shadow
  extern const uint16_t INK;          // primary text ink (dark navy, not pure black)
  extern const uint16_t PAPER;        // primary text on dark panels (warm near-white)
  extern const uint16_t GOLD;         // coins
  extern const uint16_t RED_ACCENT;   // RED button hints / danger
  extern const uint16_t GREEN_ACCENT; // GREEN button hints / success
  extern const int BODY_COUNT;
  extern const uint16_t BODY[6];      // pet color cosmetic bases
}

// Lightens (factor > 0) or darkens (factor < 0) toward white/black.
// factor is clamped to [-1, 1]. This is how every shadow/highlight tone in
// the game is produced from a single base color.
uint16_t shade(uint16_t color565, float factor);

// ---------------------------------------------------------------------------
// Sprite-map rendering: pixel art authored as arrays of strings, one char per
// pixel. A parallel legend maps chars to colors; ' ' is always transparent.
// ---------------------------------------------------------------------------
struct SpriteLegend { char key; uint16_t color; };

void drawSpriteMap(TFT_eSprite &dst, const char *const *rows, int rowCount,
                    int x, int y, int scale,
                    const SpriteLegend *legend, int legendCount);

int spriteMapWidth(const char *const *rows, int scale);
int spriteMapHeight(int rowCount, int scale);

// ---------------------------------------------------------------------------
// Panels: 9-slice-ish menu boxes with a raised light/dark border, the
// Pokemon/Fire-Emblem dialogue box look.
// ---------------------------------------------------------------------------
void drawPanel(TFT_eSprite &dst, int x, int y, int w, int h);
// Same panel, with a 1px accent trim on the top/left edge (premium-tier cells, etc).
void drawPanelAccent(TFT_eSprite &dst, int x, int y, int w, int h, uint16_t accent);
void drawPanelTitled(TFT_eSprite &dst, int x, int y, int w, int h, const char *title);

// ---------------------------------------------------------------------------
// Chunky bitmap text: TFT_eSPI's GLCD font (font 1) scaled with setTextSize,
// plus a 1px drop shadow for readability on busy backgrounds. This is the
// ONLY font used anywhere in the UI - no smooth/anti-aliased fonts.
// ---------------------------------------------------------------------------
void drawPixelText(TFT_eSprite &dst, const char *text, int x, int y, int size,
                    uint16_t color, uint16_t shadowColor);
void drawPixelTextC(TFT_eSprite &dst, const char *text, int cx, int y, int size,
                     uint16_t color, uint16_t shadowColor); // centered on cx
int pixelTextWidth(const char *text, int size);

// ---------------------------------------------------------------------------
// Backgrounds
// ---------------------------------------------------------------------------
void drawDitherGradient(TFT_eSprite &dst, uint16_t colorA, uint16_t colorB, int y0, int y1);

// ---------------------------------------------------------------------------
// Particles: small squares that burst outward and fall, used for coin
// rewards / level-up celebration.
// ---------------------------------------------------------------------------
#define MAX_PARTICLES 16
void particlesReset();
void particlesSpawnBurst(int x, int y, uint16_t color, int count);
void particlesTick(float dt);
void particlesRender(TFT_eSprite &dst);

// ---------------------------------------------------------------------------
// Toasts: slide-in banners for achievement unlocks etc. Non-blocking - the
// screen underneath keeps running.
// ---------------------------------------------------------------------------
void toastShow(const char *line1, const char *line2, uint16_t accent);
void toastTick(float dt);
void toastRender(TFT_eSprite &dst);
