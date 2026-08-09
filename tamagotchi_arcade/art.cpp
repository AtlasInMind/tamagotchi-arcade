#include <Arduino.h>
#include <math.h>
#include "art.h"
#include "cat_sprites.h"
#include "premium_assets.h"

static void drawPremiumBitmap(TFT_eSprite &spr, const uint16_t *pixels,
                              int width, int height, int x, int y, int scale,
                              bool transparent) {
  for (int py = 0; py < height; py++) {
    for (int px = 0; px < width; px++) {
      uint16_t color = pgm_read_word(pixels + py * width + px);
      if (transparent && color == 0) continue;
      spr.fillRect(x + px * scale, y + py * scale, scale, scale, color);
    }
  }
}

// ---------------------------------------------------------------------------
// Shared difficulty ramp: every category uses the same 10-step curve, so the
// grind feels consistent everywhere. Index 0 is always free/pre-owned.
// Levels climb 0..20, prices climb 0..1600 - a top item needs sustained
// play, not one lucky hand.
// ---------------------------------------------------------------------------
const int BACKGROUND_LEVELS[BACKGROUND_COUNT] = { 0, 1, 2, 4, 6, 8, 10, 13, 16, 20 };
const int HAT_LEVELS[HAT_COUNT]               = { 0, 1, 2, 4, 6, 8, 10, 13, 16, 20 };
const int ACCESSORY_LEVELS[ACCESSORY_COUNT]   = { 0, 1, 2, 4, 6, 8, 10, 13, 16, 20 };
const int PATTERN_LEVELS[PATTERN_COUNT]       = { 0, 1, 2, 4, 6, 8, 10, 13, 16, 20 };

const int BACKGROUND_PRICES[BACKGROUND_COUNT] = { 0, 30, 60, 120, 220, 350, 500, 750, 1100, 1600 };
const int HAT_PRICES[HAT_COUNT]               = { 0, 30, 60, 120, 220, 350, 500, 750, 1100, 1600 };
const int ACCESSORY_PRICES[ACCESSORY_COUNT]   = { 0, 30, 60, 120, 220, 350, 500, 750, 1100, 1600 };
const int PATTERN_PRICES[PATTERN_COUNT]       = { 0, 30, 60, 120, 220, 350, 500, 750, 1100, 1600 };

const char *BACKGROUND_NAMES[BACKGROUND_COUNT] = {
  "Dusk", "Starry Night", "Meadow", "Sunset Hills", "Snowfall",
  "Neon Grid", "Beach", "Aurora", "Volcano", "Galaxy",
};
const char *HAT_NAMES[HAT_COUNT] = {
  "No Hat", "Crimson Cap", "Moon Beanie", "Straw Hat", "Flower Crown",
  "Star Wizard", "Velvet Top Hat", "Golden Halo", "Jeweled Crown", "Royal Crown",
};
const char *ACCESSORY_NAMES[ACCESSORY_COUNT] = {
  "None", "Silk Bowtie", "Gold Pendant", "Round Glasses", "Knit Scarf",
  "Sunglasses", "Leather Pack", "Hero Medal", "Crimson Cape", "Royal Cape",
};
const char *PATTERN_NAMES[PATTERN_COUNT] = {
  "None", "Freckles", "Spots", "Stripes", "Big Spots",
  "Stars", "Hearts", "Diamonds", "Patches", "Sparkle",
};

const char *FUR_COLOR_NAMES[FUR_COLOR_COUNT] = {
  "Calico", "Cream", "Orange Tabby", "Silver", "Charcoal",
  "Midnight", "Chocolate", "Blue Gray", "Ginger", "Snow",
};
static const uint16_t FUR_COLORS[FUR_COLOR_COUNT] = {
  0xE489, // calico keeps the authored gray/orange palette (special-cased)
  0xF6B5, // cream
  0xE489, // orange tabby
  0xA514, // silver
  0x630C, // charcoal
  0x2948, // midnight navy-black
  0x7226, // chocolate
  0x5B31, // blue gray
  0xEBC6, // ginger
  0xFF79, // snow
};

// ---------------------------------------------------------------------------
// The cat. One companion, one silhouette - chosen once at character
// creation (fur color) rather than picked from several body shapes. That is
// the actual fix for "items don't fit": there's nothing left for a hat or
// accessory to fail to fit, since there's only ever one head to design
// against. Pose and proportions take cues from chibi pixel-pet art (big
// round head, big ears, two-tone coat, sitting pose, simple dot eyes).
//
// Legend: 'o' navy outline, 'B' coat midtone, 'H' coat highlight, 'S' coat
// shadow, 'P' warm cream chest/muzzle and 'K' pink inner ear.  Three fur
// tones make the tiny sprite read like authored 16-bit art instead of a
// flat mascot, while retaining a single silhouette for every cosmetic.
// ---------------------------------------------------------------------------
static const char *CAT_BODY[] = {
  "    oooo   oooo    ", //  0 ear tips
  "    oKKo   oKKo    ", //  1 inner ears
  "   oHKKBo oBKKSo   ", //  2
  "    oHHo   oSSo    ", //  3
  "    oooo   oooo    ", //  4
  "       ooooo       ", //  5 ears resolve into head
  "     ooHHBBBoo     ", //  6 forehead light
  "    oHHBBBBSSBo    ", //  7 <- eyes sit here
  "   oHHBBBBBBSSSoo  ", //  8 cheek shading
  "   oHBBBBBBBSSSSoo ", //  9
  "   oHBBPPPPPSSSoBSo", // 10 muzzle; tail starts at right
  "    oBKPPPPPKSo oSo", // 11 blush
  "     oBPPPPPSo  oSo", // 12
  "    oPPPPPPPPPooBSo", // 13
  "   oHPPPPPPPPPSSSSo", // 14 chest/belly
  "   oHPPPPPPPPPSSSo ", // 15
  "   oHPPPPPPPPPSSSo ", // 16 paws
  "    ooooooooooooo  ", // 17
};
#define CAT_ROWS 18
#define CAT_COLS 19

// Anchor points, in the grid above. Only one body now, so these are plain
// constants rather than a per-species table.
#define HEAD_AX 9
#define HEAD_AY 0   // top of the ear tips - hats are sized to cover the whole head+ears
#define CHEST_AX 9
#define CHEST_AY 10
#define FACE_AX 9
#define FACE_AY 7

// ---------------------------------------------------------------------------
// Face overlay: simple dot eyes (matching the reference chibi-cat style)
// plus a mouth, anchored at the eye row.
// ---------------------------------------------------------------------------
static void drawPetFace(TFT_eSprite &spr, int anchorX, int anchorY, int scale, PetExpression expr, uint16_t outline) {
  auto px = [&](int dx, int dy, int w, int h, uint16_t color) {
    spr.fillRect(anchorX + dx * scale, anchorY + dy * scale, w * scale, h * scale, color);
  };

  bool blink = (expr == EXPR_BLINK);
  if (blink) {
    px(-5, 1, 3, 1, outline);
    px(2, 1, 3, 1, outline);
  } else {
    // Large two-pixel eyes with warm highlights: readable even at scale 2
    // in the shop, expressive at scale 5 on the home screen.
    px(-5, 0, 2, 2, outline);
    px(3, 0, 2, 2, outline);
    if (scale >= 3 && expr != EXPR_SAD) {
      spr.fillRect(anchorX - 5 * scale, anchorY, scale, scale, Pal::PAPER);
      spr.fillRect(anchorX + 3 * scale, anchorY, scale, scale, Pal::PAPER);
    }
  }

  switch (expr) {
    case EXPR_HAPPY:
      px(-4, 3, 1, 1, outline);
      px(-3, 4, 3, 1, outline);
      px(2, 3, 1, 1, outline);
      break;
    case EXPR_EXCITED:
      // Open mouth with a pink tongue and tiny excitement sparks.
      px(-2, 3, 4, 2, outline);
      px(-1, 4, 2, 1, TFT_PINK);
      px(-8, -2, 1, 2, Pal::GOLD);
      px(7, -2, 1, 2, Pal::GOLD);
      break;
    case EXPR_SAD:
      px(-2, 3, 3, 1, outline);
      px(-4, 4, 1, 1, outline);
      px(2, 4, 1, 1, outline);
      break;
    default: // neutral / blink
      px(-1, 3, 2, 1, outline);
      break;
  }
}

// ---------------------------------------------------------------------------
// Hats. Anchored at (anchorX, anchorY) = top of the ear tips, in absolute
// pixels; dy grows downward from there. Every hat here is built from
// `row()`, which fills one horizontal strip `halfW` grid-units either side
// of center - i.e. every hat is an explicit, symmetric, gap-free silhouette
// rather than a scatter of rectangles, so it reads as one solid shape.
// Because there is only one head shape in the whole game, these were
// measured directly against CAT_BODY above and don't need to generalize.
// ---------------------------------------------------------------------------
static void drawPremiumHatLayer(TFT_eSprite &spr, int anchorX, int anchorY,
                                int scale, int hatIndex, bool frontLayer) {
  if (hatIndex <= 0 || hatIndex >= HAT_COUNT) return;

  // Rows at/after this boundary are the portion that physically crosses
  // the forehead. Everything above it belongs behind the ears/head.
  // Flower crown is all-front; halo is all-back.
  static const uint8_t FRONT_ROW[HAT_COUNT] = {
    22, 19, 19, 18, 0, 19, 20, 22, 19, 18
  };
  static const int8_t Y_SHIFT[HAT_COUNT] = {0, 0, 0, 0, 0, 0, 0, -14, 0, -2};

  int x0 = anchorX - (PREMIUM_HAT_W * scale) / 2;
  int y0 = anchorY - 10 * scale + Y_SHIFT[hatIndex] * scale;
  int split = FRONT_ROW[hatIndex];
  int firstRow = frontLayer ? split : 0;
  int lastRow = frontLayer ? PREMIUM_HAT_H : split;
  for (int py = firstRow; py < lastRow; py++) {
    for (int px = 0; px < PREMIUM_HAT_W; px++) {
      uint16_t color = pgm_read_word(PREMIUM_HATS[hatIndex - 1] + py * PREMIUM_HAT_W + px);
      if (color == 0) continue;
      spr.fillRect(x0 + px * scale, y0 + py * scale, scale, scale, color);
    }
  }
}

void drawHat(TFT_eSprite &spr, int anchorX, int anchorY, int scale, int hatIndex) {
  // Index zero is deliberately empty. The previous default branch turned
  // it into a crown, which made "None" impossible to equip.
  if (hatIndex == 0) return;
  if (hatIndex > 0 && hatIndex < HAT_COUNT) {
    // Standalone/shop preview: render both layers with no cat between them.
    drawPremiumHatLayer(spr, anchorX, anchorY, scale, hatIndex, false);
    drawPremiumHatLayer(spr, anchorX, anchorY, scale, hatIndex, true);
    return;
  }
  auto row = [&](int dy, int halfW, uint16_t color) {
    spr.fillRect(anchorX - halfW * scale, anchorY + dy * scale, halfW * 2 * scale, scale, color);
  };
  auto dot = [&](int dx, int dy, int w, int h, uint16_t color) {
    spr.fillRect(anchorX + dx * scale, anchorY + dy * scale, w * scale, h * scale, color);
  };

  switch (hatIndex) {
    case 1: { // Cap - rounded dome + brim, covers ears entirely
      uint16_t c = TFT_RED, dark = shade(TFT_RED, -0.35f);
      int hw[] = { 3, 5, 6, 7, 7, 7, 7 }; // dy 0..6
      for (int i = 0; i < 7; i++) row(i, hw[i], c);
      row(7, 8, dark);            // brim
      dot(2, 6, 3, 1, dark);       // brim shadow line
      dot(-1, -1, 2, 2, dark);     // button on top
      break;
    }
    case 2: { // Beanie - snug dome, folded cuff, pompom
      uint16_t c = 0x2D9F, cuff = shade(c, -0.3f);
      int hw[] = { 2, 4, 6, 7, 7, 7 }; // dy 0..5
      for (int i = 0; i < 6; i++) row(i, hw[i], c);
      row(6, 7, cuff); // folded cuff at the bottom
      spr.fillCircle(anchorX, anchorY - scale, scale + scale / 2, TFT_WHITE); // pompom
      break;
    }
    case 3: { // Bucket Hat - dome + wide flat brim
      uint16_t c = 0x1E64, dark = shade(c, -0.35f);
      int hw[] = { 2, 4, 5, 6, 7 }; // dy 0..4
      for (int i = 0; i < 5; i++) row(i, hw[i], c);
      row(5, 9, dark);  // wide brim
      row(6, 9, dark);
      break;
    }
    case 4: { // Flower Crown - band at ear-base, ears peek through above
      row(4, 7, 0x2E64);
      row(5, 7, shade(0x2E64, -0.3f));
      dot(-7, 3, 2, 2, TFT_PINK);
      dot(-2, 2, 2, 2, TFT_YELLOW);
      dot(3, 3, 2, 2, TFT_PINK);
      dot(6, 4, 2, 2, TFT_MAGENTA);
      break;
    }
    case 5: { // Wizard Hat - tall cone, wide brim over the ears
      uint16_t c = 0x50D3, dark = shade(c, -0.35f);
      row(8, 8, dark); // brim, at the head's widest point
      int hw[] = { 1, 1, 2, 2, 3, 3, 4, 5, 6 }; // dy -8..0 (apex to base)
      for (int i = 0; i < 9; i++) row(i - 8, hw[i], c);
      dot(-1, -9, 2, 2, TFT_YELLOW); // star tip
      break;
    }
    case 6: { // Top Hat - narrow cylinder + brim
      uint16_t c = TFT_BLACK, band = TFT_RED;
      row(8, 8, shade(c, 0.15f)); // brim
      for (int i = -8; i <= 7; i++) row(i, 5, c);
      row(6, 5, band); // hat band
      break;
    }
    case 7: { // Halo - floats above the head, never touches it (always safe)
      spr.drawCircle(anchorX, anchorY - 8 * scale, 6 * scale, TFT_YELLOW);
      spr.drawCircle(anchorX, anchorY - 8 * scale, 6 * scale - 1, TFT_YELLOW);
      break;
    }
    case 8: { // Crown - jeweled band at ear-base, spikes above
      row(4, 7, TFT_GOLD);
      row(5, 7, shade(TFT_GOLD, -0.4f));
      dot(-7, 1, 2, 3, TFT_GOLD);
      dot(-2, 0, 2, 4, TFT_GOLD);
      dot(3, 1, 2, 3, TFT_GOLD);
      dot(-1, 2, 2, 2, TFT_RED); // central gem
      break;
    }
    default: { // Golden Crown (premium) - bigger, gemmed, fuller band
      row(3, 8, TFT_GOLD);
      row(4, 8, TFT_GOLD);
      row(5, 8, shade(TFT_GOLD, -0.4f));
      dot(-8, -1, 2, 4, TFT_GOLD);
      dot(-2, -2, 2, 5, TFT_GOLD);
      dot(4, -1, 2, 4, TFT_GOLD);
      dot(-1, 2, 2, 2, TFT_RED);
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Accessories. Most anchor at (chestX, chestY) = top of the chest, dy
// positive going down. Glasses/sunglasses anchor at (faceX, faceY) instead
// - the eye row - so they always sit exactly on the eyes.
// ---------------------------------------------------------------------------
static void drawPremiumAccessoryLayer(TFT_eSprite &spr, int chestX, int chestY,
                                      int faceX, int faceY, int scale,
                                      int accIndex, bool frontLayer) {
  if (accIndex <= 0 || accIndex >= ACCESSORY_COUNT) return;
  bool faceItem = accIndex == 3 || accIndex == 5;
  int anchorX = faceItem ? faceX : chestX;
  int x0 = anchorX - (PREMIUM_ACC_W * scale) / 2;
  int y0 = 0;
  switch (accIndex) {
    case 1: y0 = chestY - 20 * scale; break;
    case 2: y0 = chestY - 14 * scale; break;
    case 3: y0 = faceY - 21 * scale; break;
    case 4: y0 = chestY - 14 * scale; break;
    case 5: y0 = faceY - 22 * scale; break;
    case 6: x0 += 10 * scale; y0 = chestY - 18 * scale; break;
    case 7: y0 = chestY - 12 * scale; break;
    case 8: y0 = chestY - 8 * scale; break;
    case 9: y0 = chestY - 7 * scale; break;
  }

  // Let the hanging end of the scarf peek out beside the body while its
  // neck wrap remains centred on the cat.
  if (!frontLayer && accIndex == 4) x0 += 4 * scale;

  int firstRow = 0, lastRow = PREMIUM_ACC_H;
  if (!frontLayer) {
    if (accIndex != 4 && accIndex != 6 && accIndex != 8 && accIndex != 9) return;
  } else {
    if (accIndex == 6) return;                  // backpack is entirely behind
    if (accIndex == 4) { firstRow = 12; lastRow = 18; } // scarf neck wrap only
    if (accIndex == 8 || accIndex == 9) {
      // The broad source-image collar belongs behind the head. A compact
      // clasp is authored here so the cape attaches cleanly at the chest.
      uint16_t cloth = accIndex == 8 ? 0xB9C7 : 0x04BF;
      spr.fillRect(chestX - 2 * scale, chestY - 2 * scale,
                   5 * scale, 2 * scale, cloth);
      spr.fillRect(chestX, chestY - 2 * scale, scale, scale, 0xF5ED);
      return;
    }
  }

  for (int py = firstRow; py < lastRow; py++) {
    for (int px = 0; px < PREMIUM_ACC_W; px++) {
      uint16_t color = pgm_read_word(PREMIUM_ACCESSORIES[accIndex - 1] + py * PREMIUM_ACC_W + px);
      if (color == 0) continue;
      spr.fillRect(x0 + px * scale, y0 + py * scale, scale, scale, color);
    }
  }
}

void drawAccessory(TFT_eSprite &spr, int chestX, int chestY, int faceX, int faceY, int scale, int accIndex) {
  if (accIndex == 0) return;
  if (accIndex > 0 && accIndex < ACCESSORY_COUNT) {
    drawPremiumAccessoryLayer(spr, chestX, chestY, faceX, faceY, scale, accIndex, false);
    drawPremiumAccessoryLayer(spr, chestX, chestY, faceX, faceY, scale, accIndex, true);
    return;
  }
  auto pxAt = [&](int originX, int originY, int dx, int dy, int w, int h, uint16_t color) {
    spr.fillRect(originX + dx * scale, originY + dy * scale, w * scale, h * scale, color);
  };
  auto px = [&](int dx, int dy, int w, int h, uint16_t color) { pxAt(chestX, chestY, dx, dy, w, h, color); };
  auto pxFace = [&](int dx, int dy, int w, int h, uint16_t color) { pxAt(faceX, faceY, dx, dy, w, h, color); };

  switch (accIndex) {
    case 1: // Bowtie
      px(-2, 0, 2, 2, TFT_RED);
      px(1, 0, 2, 2, TFT_RED);
      px(0, 0, 1, 2, shade(TFT_RED, -0.4f));
      break;
    case 2: // Necklace
      spr.drawFastHLine(chestX - 5 * scale, chestY, 10 * scale, TFT_GOLD);
      px(-1, 1, 2, 2, 0x2D9F);
      break;
    case 3: // Glasses - anchored at eye level
      pxFace(-6, 0, 3, 2, TFT_BLACK);
      pxFace(3, 0, 3, 2, TFT_BLACK);
      pxFace(-3, 0, 6, 1, TFT_BLACK);
      break;
    case 4: // Scarf
      px(-5, 0, 11, 2, TFT_ORANGE);
      px(4, 1, 2, 3, TFT_ORANGE);
      break;
    case 5: // Sunglasses - anchored at eye level
      pxFace(-6, 0, 3, 2, shade(TFT_BLACK, 0.15f));
      pxFace(3, 0, 3, 2, shade(TFT_BLACK, 0.15f));
      pxFace(-3, 0, 6, 1, TFT_BLACK);
      break;
    case 6: // Backpack
      px(4, -1, 4, 6, 0x8B0E);
      px(4, 0, 4, 1, shade(0x8B0E, -0.3f));
      break;
    case 7: // Medal
      spr.drawFastVLine(chestX - 2 * scale, chestY - 4 * scale, 4 * scale, TFT_YELLOW);
      spr.drawFastVLine(chestX + 2 * scale, chestY - 4 * scale, 4 * scale, TFT_YELLOW);
      px(-3, 0, 6, 3, TFT_GOLD);
      px(-1, 1, 2, 1, TFT_YELLOW);
      break;
    case 8: // Cape
      px(-6, -1, 3, 8, 0x8000);
      px(4, -1, 3, 8, 0x8000);
      px(-6, -1, 12, 1, shade(0x8000, -0.3f));
      break;
    case 9: // Royal Cape (premium: gold trim, bigger)
      px(-7, -1, 4, 9, 0x8000);
      px(4, -1, 4, 9, 0x8000);
      px(-7, -1, 14, 1, TFT_GOLD);
      px(-7, 7, 4, 1, TFT_GOLD);
      px(4, 7, 4, 1, TFT_GOLD);
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Patterns: small stamp fields sampled against CAT_BODY, so marks only ever
// land on actual coat pixels (never on the white chest/muzzle or outline).
// ---------------------------------------------------------------------------
struct StampPoint { float fx, fy; };
static const StampPoint STAMPS_SCATTER[] = { {0.30f,0.40f},{0.65f,0.38f},{0.25f,0.55f},{0.72f,0.52f},{0.40f,0.30f},{0.58f,0.60f} };
static const StampPoint STAMPS_BIG[]     = { {0.30f,0.42f},{0.68f,0.42f},{0.50f,0.30f} };
static const StampPoint STAMPS_BANDS[]   = { {0.25f,0.35f},{0.40f,0.35f},{0.60f,0.35f},{0.75f,0.35f},{0.30f,0.50f},{0.68f,0.50f} };

static void drawPatternStamps(TFT_eSprite &spr, int x0, int y0, int scale,
                               const StampPoint *pts, int count, uint16_t color, int dotSize, bool useStar) {
  for (int i = 0; i < count; i++) {
    int col = (int)(pts[i].fx * CAT_COLS);
    int row = (int)(pts[i].fy * CAT_ROWS);
    if (row < 0 || row >= CAT_ROWS || col < 0 || col >= CAT_COLS) continue;
    char c = CAT_BODY[row][col];
    if (c != 'B') continue; // only stamp onto actual coat fill, never the white chest/muzzle
    int px = x0 + col * scale, py = y0 + row * scale;
    if (useStar) drawStarIcon(spr, px, py, 1, color);
    else spr.fillRect(px, py, dotSize * scale, dotSize * scale, color);
  }
}

static void drawPattern(TFT_eSprite &spr, int x0, int y0, int scale, int patternIndex) {
  auto coatPixel = [&](int col, int row, uint16_t color) {
    if (row < 0 || row >= CAT_ROWS || col < 0 || col >= CAT_COLS) return;
    char c = CAT_BODY[row][col];
    if (c == 'B' || c == 'H' || c == 'S')
      spr.fillRect(x0 + col * scale, y0 + row * scale, scale, scale, color);
  };
  uint16_t dark = shade(Pal::INK, 0.18f);
  switch (patternIndex) {
    case 1: drawPatternStamps(spr, x0, y0, scale, STAMPS_SCATTER, 6, Pal::PAPER, 1, false); break;              // Freckles
    case 2: drawPatternStamps(spr, x0, y0, scale, STAMPS_SCATTER, 6, shade(Pal::INK, 0.2f), 2, false); break;    // Spots
    case 3: { // Tabby: forehead M, cheek bars, back and tail bands
      int marks[][2] = {{7,6},{9,6},{11,6},{8,7},{10,7},{4,8},{14,8},
                        {4,9},{14,9},{5,14},{13,14},{16,10},{17,12},{16,14}};
      for (unsigned i = 0; i < sizeof(marks) / sizeof(marks[0]); i++)
        coatPixel(marks[i][0], marks[i][1], dark);
      break;
    }
    case 4: drawPatternStamps(spr, x0, y0, scale, STAMPS_BIG, 3, Pal::PAPER, 3, false); break;                   // Big Spots
    case 5: drawPatternStamps(spr, x0, y0, scale, STAMPS_SCATTER, 6, TFT_YELLOW, 1, true); break;                // Stars
    case 6: { // Three tiny, actual heart silhouettes
      int hearts[][2] = {{6,8},{12,8},{15,14}};
      for (int i = 0; i < 3; i++) {
        int c = hearts[i][0], r = hearts[i][1];
        coatPixel(c - 1, r, Pal::RED_ACCENT); coatPixel(c + 1, r, Pal::RED_ACCENT);
        coatPixel(c, r + 1, Pal::RED_ACCENT);
      }
      break;
    }
    case 7: { // Diamonds
      int diamonds[][2] = {{6,8},{12,8},{15,14}};
      for (int i = 0; i < 3; i++) {
        int c = diamonds[i][0], r = diamonds[i][1];
        coatPixel(c, r - 1, 0x2D9F); coatPixel(c - 1, r, 0x2D9F);
        coatPixel(c + 1, r, 0x2D9F); coatPixel(c, r + 1, 0x2D9F);
      }
      break;
    }
    case 8: drawPatternStamps(spr, x0, y0, scale, STAMPS_BIG, 3, shade(Pal::INK, 0.35f), 3, false); break;       // Patches
    case 9: drawPatternStamps(spr, x0, y0, scale, STAMPS_SCATTER, 6, TFT_WHITE, 1, true); break;                 // Sparkle (premium)
    default: break;
  }
}

// ---------------------------------------------------------------------------
void drawPet(TFT_eSprite &spr, int cx, int cy, int scale, PetExpression expr,
             int furColorIndex, int patternIndex,
             int hatIndex, int accessoryIndex, int bobOffset) {
  // The original API used a 19x18 cat at scale 5. The approved sprite is
  // 30x40, so translate legacy call-site scales to equivalent on-screen
  // sizes without requiring every screen to know the asset dimensions.
  int pxScale = scale >= 5 ? 3 : (scale >= 3 ? 2 : 1);
  int x0 = cx - (CAT_SPRITE_W * pxScale) / 2;
  int y0 = cy - (CAT_SPRITE_H * pxScale) / 2 + bobOffset;
  int frame = expr == EXPR_ASLEEP ? EXPR_BLINK
                                  : constrain((int)expr, 0, CAT_SPRITE_FRAME_COUNT - 1);
  furColorIndex = constrain(furColorIndex, 0, FUR_COLOR_COUNT - 1);

  int headPxX = x0 + 15 * pxScale, headPxY = y0 + 2 * pxScale;
  int chestPxX = x0 + 15 * pxScale, chestPxY = y0 + 23 * pxScale;
  int facePxX = x0 + 15 * pxScale, facePxY = y0 + 17 * pxScale;

  drawPremiumAccessoryLayer(spr, chestPxX, chestPxY, facePxX, facePxY,
                            pxScale, accessoryIndex, false);

  const uint8_t *pixels = CAT_SPRITE_FRAMES[frame];
  auto catColor = [&](uint8_t index) {
    uint16_t color = pgm_read_word(CAT_SPRITE_PALETTE + index);
    if (furColorIndex != 0 && index >= 3 && index <= 9) {
      static const float FUR_SHADE[7] = {-0.48f, -0.26f, 0.08f, -0.55f, -0.30f, 0.0f, 0.25f};
      color = shade(FUR_COLORS[furColorIndex], FUR_SHADE[index - 3]);
    }
    return color;
  };
  bool hideEars = hatIndex == 1 || hatIndex == 2 || hatIndex == 3 ||
                  hatIndex == 5 || hatIndex == 8 || hatIndex == 9;
  auto isEarPixel = [&](int x, int y) {
    return y >= 1 && y <= 10 && ((x >= 3 && x <= 10) || (x >= 20 && x <= 26));
  };
  for (int y = 0; y < CAT_SPRITE_H; y++) {
    for (int x = 0; x < CAT_SPRITE_W; x++) {
      uint8_t index = pgm_read_byte(pixels + y * CAT_SPRITE_W + x);
      if (index == 0) continue;
      if (hideEars && isEarPixel(x, y)) continue;
      spr.fillRect(x0 + x * pxScale, y0 + y * pxScale, pxScale, pxScale, catColor(index));
    }
  }

  // Pattern pixels are clipped to authored fur palette regions. Eyes,
  // muzzle, chest, inner ears and outline therefore remain untouched.
  if (patternIndex > 0) {
    auto furPixel = [&](int px, int py, uint16_t color) {
      if (px < 0 || px >= CAT_SPRITE_W || py < 0 || py >= CAT_SPRITE_H) return;
      uint8_t source = pgm_read_byte(pixels + py * CAT_SPRITE_W + px);
      if (source < 3 || source > 9) return;
      spr.fillRect(x0 + px * pxScale, y0 + py * pxScale, pxScale, pxScale, color);
    };
    uint16_t dark = furColorIndex == 5 ? shade(Pal::PAPER, -0.35f) : shade(Pal::INK, 0.28f);
    static const int8_t BASE_MARKS[][2] = {{7,17},{22,17},{6,27},{23,27},{9,32},{21,31}};
    if (patternIndex == 1) { // freckles
      int8_t p[][2]={{8,17},{10,18},{20,18},{22,17},{7,28},{22,29}};
      for (auto &m : p) furPixel(m[0], m[1], shade(Pal::GOLD, -0.35f));
    } else if (patternIndex == 2) { // spots
      for (auto &m : BASE_MARKS) {
        furPixel(m[0],m[1],dark); furPixel(m[0]+1,m[1],dark); furPixel(m[0],m[1]+1,dark);
      }
    } else if (patternIndex == 3) { // tabby stripes: forehead, cheeks, legs and tail
      int8_t p[][2]={{12,7},{15,6},{18,7},{5,16},{6,18},{24,16},{23,18},
                     {7,27},{8,29},{22,27},{21,29},{27,27},{27,31},{26,35}};
      for (auto &m : p) furPixel(m[0],m[1],dark);
    } else if (patternIndex == 4 || patternIndex == 8) { // large spots / patches
      uint16_t c = patternIndex == 8 ? shade(Pal::INK, 0.15f) : shade(Pal::PAPER, -0.12f);
      int8_t centers[][2]={{7,16},{22,16},{7,29},{22,29}};
      for (auto &m : centers)
        for (int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++) furPixel(m[0]+dx,m[1]+dy,c);
    } else if (patternIndex == 5 || patternIndex == 9) { // stars / magical sparkle
      uint16_t c = patternIndex == 9 ? TFT_WHITE : Pal::GOLD;
      for (auto &m : BASE_MARKS) {
        furPixel(m[0],m[1],c); furPixel(m[0]-1,m[1],c); furPixel(m[0]+1,m[1],c);
        furPixel(m[0],m[1]-1,c); furPixel(m[0],m[1]+1,c);
      }
    } else if (patternIndex == 6) { // hearts
      int8_t centers[][2]={{7,17},{22,17},{8,29},{21,29}};
      for (auto &m : centers) {
        furPixel(m[0]-1,m[1],Pal::RED_ACCENT); furPixel(m[0]+1,m[1],Pal::RED_ACCENT);
        furPixel(m[0],m[1]+1,Pal::RED_ACCENT);
      }
    } else if (patternIndex == 7) { // diamonds
      for (auto &m : BASE_MARKS) {
        furPixel(m[0],m[1]-1,0x3D9F); furPixel(m[0]-1,m[1],0x3D9F);
        furPixel(m[0]+1,m[1],0x3D9F); furPixel(m[0],m[1]+1,0x3D9F);
      }
    }
  }

  // Draw the complete, uncut hat. Then restore only the authored ear pixels
  // that should occlude it. This creates natural depth without slicing the
  // hat bitmap into visibly disconnected horizontal pieces.
  drawHat(spr, headPxX, headPxY, pxScale, hatIndex);
  static const uint8_t EAR_FRONT_MAX_Y[HAT_COUNT] = {0, 0, 0, 0, 0, 0, 6, 0, 0, 0};
  int earMaxY = (hatIndex >= 0 && hatIndex < HAT_COUNT) ? EAR_FRONT_MAX_Y[hatIndex] : 0;
  if (earMaxY > 0) {
    for (int y = 1; y <= earMaxY; y++) {
      for (int x = 3; x <= 26; x++) {
        if (!isEarPixel(x, y)) continue;
        uint8_t index = pgm_read_byte(pixels + y * CAT_SPRITE_W + x);
        if (index == 0) continue;
        spr.fillRect(x0 + x * pxScale, y0 + y * pxScale,
                     pxScale, pxScale, catColor(index));
      }
    }
  }

  drawPremiumAccessoryLayer(spr, chestPxX, chestPxY, facePxX, facePxY,
                            pxScale, accessoryIndex, true);

  if (expr == EXPR_ASLEEP) {
    // Three clean pixel-Z glyphs drift diagonally above the tail. The cat
    // itself uses the authored blink frame, which reads naturally as sleep.
    auto z = [&](int zx, int zy, int s) {
      spr.fillRect(zx, zy, 3 * s, s, Pal::PAPER);
      spr.fillRect(zx + 2 * s, zy + s, s, s, Pal::PAPER);
      spr.fillRect(zx + s, zy + 2 * s, s, s, Pal::PAPER);
      spr.fillRect(zx, zy + 3 * s, 3 * s, s, Pal::PAPER);
    };
    int phase = (millis() / 700) % 3;
    z(x0 + 25 * pxScale, y0 + (12 - phase) * pxScale, pxScale);
    z(x0 + 28 * pxScale, y0 + (6 - phase) * pxScale, max(1, pxScale - 1));
  }
}

// ---------------------------------------------------------------------------
// Backgrounds - layered scenes with a light ambient touch.
// ---------------------------------------------------------------------------
static void ambientDrift(TFT_eSprite &spr, float dt, uint16_t color, int count, bool snowLike) {
  static float ax[16], ay[16];
  static bool seeded = false;
  if (!seeded) {
    for (int i = 0; i < 16; i++) { ax[i] = random(SCREEN_W); ay[i] = random(SCREEN_H); }
    seeded = true;
  }
  for (int i = 0; i < count && i < 16; i++) {
    ay[i] += (snowLike ? 14.0f : 10.0f) * dt;
    ax[i] += sinf(ay[i] * 0.05f) * 6.0f * dt;
    if (ay[i] > SCREEN_H) { ay[i] = -4; ax[i] = random(SCREEN_W); }
    spr.fillRect((int)ax[i], (int)ay[i], 2, 2, color);
  }
}

static void drawCloud(TFT_eSprite &spr, int x, int y, uint16_t color) {
  spr.fillRect(x + 5, y, 14, 4, color);
  spr.fillRect(x, y + 4, 25, 5, color);
  spr.fillRect(x + 3, y + 9, 19, 2, shade(color, -0.12f));
}

static void drawPine(TFT_eSprite &spr, int x, int groundY, int size, uint16_t color) {
  spr.fillRect(x - 1, groundY - size, 3, size, 0x8A42);
  spr.fillTriangle(x, groundY - size * 2, x - size / 2, groundY - size / 2,
                   x + size / 2, groundY - size / 2, shade(color, 0.12f));
  spr.fillTriangle(x, groundY - size * 3 / 2, x - size * 2 / 3, groundY - 2,
                   x + size * 2 / 3, groundY - 2, color);
}

static void scatterGround(TFT_eSprite &spr, int y0, uint16_t a, uint16_t b) {
  uint32_t seed = 0xC0FFEE;
  for (int i = 0; i < 70; i++) {
    seed = seed * 1664525UL + 1013904223UL;
    int x = (seed >> 8) % SCREEN_W;
    seed = seed * 1664525UL + 1013904223UL;
    int y = y0 + ((seed >> 8) % max(1, SCREEN_H - y0));
    spr.drawPixel(x, y, (i & 1) ? a : b);
  }
}

void drawBackground(TFT_eSprite &spr, int index, float dt) {
  // Approved scenes are stored at one third resolution and expanded with
  // nearest-neighbour blocks. This costs only 72 KB for all ten full-screen
  // backgrounds while preserving intentional pixel edges and composition.
  index = constrain(index, 0, BACKGROUND_COUNT - 1);
  drawPremiumBitmap(spr, PREMIUM_BACKGROUNDS[index], PREMIUM_BG_W,
                    PREMIUM_BG_H, 0, 0, 3, false);
  (void)dt;
  return;
  int w = SCREEN_W, h = SCREEN_H;
  switch (index) {
    case 0: // Dusk
      drawDitherGradient(spr, Pal::SKY_A, Pal::BG, 0, h);
      spr.fillCircle(20, 34, 7, 0xFEF6);
      spr.fillTriangle(0, h - 48, 36, h - 82, 72, h - 48, 0x1946);
      spr.fillTriangle(48, h - 48, 98, h - 95, w, h - 48, 0x2147);
      spr.fillRect(0, h - 48, w, 48, 0x1945);
      drawPine(spr, 18, h - 18, 20, 0x1BA5);
      drawPine(spr, 116, h - 12, 26, 0x1BA5);
      break;
    case 1: { // Starry Night
      spr.fillSprite(Pal::BG);
      uint32_t seed = 777;
      for (int i = 0; i < 45; i++) {
        seed = seed * 1103515245 + 12345;
        int x = (seed >> 8) % w;
        seed = seed * 1103515245 + 12345;
        int y = (seed >> 8) % (h - 40);
        spr.drawPixel(x, y, TFT_WHITE);
      }
      spr.fillCircle(w - 24, 30, 10, 0xE73C);
      spr.fillCircle(w - 20, 26, 10, Pal::BG); // crescent moon
      spr.fillRect(0, h - 48, w, 48, 0x1124);
      drawPine(spr, 14, h - 12, 25, 0x11E4);
      drawPine(spr, 121, h - 8, 30, 0x11E4);
      break;
    }
    case 2: { // Meadow
      drawDitherGradient(spr, 0x8FF9, 0x2986, 0, h - 70);
      spr.fillRect(0, h - 70, w, 70, 0x1E64);
      spr.fillRect(0, h - 72, w, 3, shade(0x1E64, 0.3f));
      drawCloud(spr, 10, 30, 0xEFFF);
      drawCloud(spr, 92, 54, 0xEFFF);
      scatterGround(spr, h - 68, 0x2F05, 0x1624);
      for (int x = 9; x < w; x += 23) {
        spr.drawFastVLine(x, h - 28, 4, 0x1624);
        spr.fillRect(x - 1, h - 30, 3, 3, (x & 1) ? 0xFBAE : 0xFEA0);
      }
      break;
    }
    case 3: { // Sunset Hills
      drawDitherGradient(spr, 0xFC48, 0x780F, 0, h - 60);
      spr.fillRect(0, h - 60, w, 60, Pal::BG);
      spr.fillTriangle(0, h - 60, 40, h - 100, 80, h - 60, shade(Pal::BG, 0.2f));
      spr.fillTriangle(50, h - 60, 100, h - 120, w, h - 60, shade(Pal::BG, 0.35f));
      spr.fillCircle(22, h - 98, 12, 0xFEA0);
      scatterGround(spr, h - 58, 0x2105, 0x2946);
      break;
    }
    case 4: // Snowfall
      drawDitherGradient(spr, 0xC618, 0x8C71, 0, h - 48);
      spr.fillRect(0, h - 48, w, 48, 0xEF7D);
      drawPine(spr, 17, h - 14, 25, 0x3B4A);
      drawPine(spr, 119, h - 12, 30, 0x3B4A);
      ambientDrift(spr, dt, TFT_WHITE, 10, true);
      break;
    case 5: { // Neon Grid
      spr.fillSprite(0x1004);
      for (int x = 0; x < w; x += 15) spr.drawFastVLine(x, 0, h, 0x2986);
      for (int y = 0; y < h; y += 15) spr.drawFastHLine(0, y, w, 0x2986);
      // Perspective horizon turns a plain grid into an arcade stage.
      spr.drawFastHLine(0, 72, w, 0xF81F);
      for (int x = 0; x < w; x += 18)
        spr.drawLine(w / 2, 72, x, h, 0x481F);
      break;
    }
    case 6: { // Beach
      drawDitherGradient(spr, 0x5D9F, 0xAEDF, 0, h - 90);
      spr.fillRect(0, h - 90, w, 90, 0xF731);
      spr.fillRect(0, h - 92, w, 3, shade(0x2D9F, 0.3f));
      drawCloud(spr, 12, 28, 0xEFFF);
      spr.fillCircle(108, 34, 11, 0xFEA0);
      for (int y = h - 88; y < h - 68; y += 6)
        spr.drawFastHLine((y & 4) ? 4 : 18, y, w - 28, 0x4D5F);
      scatterGround(spr, h - 65, 0xD5C9, 0xFEEB);
      break;
    }
    case 7: { // Aurora
      drawDitherGradient(spr, 0x2986, Pal::BG, 0, h);
      for (int i = 0; i < 3; i++) {
        int y = 30 + i * 22;
        for (int x = 0; x < w; x++) {
          int wob = (int)(sin((x + i * 30) * 0.08f) * 6);
          spr.drawPixel(x, y + wob, i % 2 ? 0x0726 : 0x8FF9);
        }
      }
      spr.fillRect(0, h - 48, w, 48, 0x1164);
      drawPine(spr, 14, h - 8, 24, 0x1A84);
      drawPine(spr, 121, h - 8, 28, 0x1A84);
      break;
    }
    case 8: { // Volcano
      drawDitherGradient(spr, 0x4000, 0x1000, 0, h - 80);
      spr.fillTriangle(w / 2, h - 140, 20, h - 80, w - 20, h - 80, 0x2104);
      spr.fillRect(0, h - 80, w, 80, 0x1000);
      spr.fillCircle(w / 2, h - 140, 5, TFT_ORANGE);
      spr.fillTriangle(w / 2 - 7, h - 135, w / 2, h - 122, w / 2 + 8, h - 135, TFT_RED);
      for (int x = 4; x < w; x += 19)
        spr.fillRect(x, h - 35 + (x % 9), 8, 2, (x & 1) ? TFT_RED : TFT_ORANGE);
      break;
    }
    default: { // Galaxy (premium)
      drawDitherGradient(spr, 0x4812, 0x1002, 0, h);
      ambientDrift(spr, dt, 0xFE19, 10, false);
      spr.fillCircle(30, 40, 14, 0x781F);
      spr.fillCircle(30, 40, 16, 0x781F);
      spr.drawCircle(95, 58, 13, 0xFEA0);
      spr.drawEllipse(95, 58, 25, 6, 0xFEA0);
      for (int i = 0; i < 18; i++)
        spr.drawPixel((i * 47) % w, (i * 83) % h, (i & 1) ? TFT_WHITE : 0x9D7F);
      break;
    }
  }
}

uint16_t backgroundSwatchColor(int index) {
  switch (index) {
    case 0: return Pal::SKY_A;
    case 1: return Pal::BG;
    case 2: return 0x2986;
    case 3: return 0xFC48;
    case 4: return 0xC618;
    case 5: return 0x1004;
    case 6: return 0xAEDF;
    case 7: return 0x2986;
    case 8: return 0x4000;
    default: return 0x4812;
  }
}

void drawBackgroundThumbnail(TFT_eSprite &spr, int x, int y, int index) {
  index = constrain(index, 0, BACKGROUND_COUNT - 1);
  // 45x24 center crop from the same atlas used by the home screen.
  for (int py = 0; py < 24; py++) {
    for (int px = 0; px < PREMIUM_BG_W; px++) {
      int sourceY = 22 + py * 2;
      uint16_t color = pgm_read_word(PREMIUM_BACKGROUNDS[index] + sourceY * PREMIUM_BG_W + px);
      spr.drawPixel(x + px, y + py, color);
    }
  }
  spr.drawRect(x - 1, y - 1, PREMIUM_BG_W + 2, 26, Pal::INK);
}

// ---------------------------------------------------------------------------
// Small icons
// ---------------------------------------------------------------------------
void drawCoinIcon(TFT_eSprite &spr, int x, int y, int scale) {
  static const char *COIN[] = {
    "..ooo..", ".oGGGo.", "oGWWGGo", "oGWGGGo",
    "oGGGGGo", ".oGGGo.", "..ooo..",
  };
  SpriteLegend l[3] = {
    {'o', shade(Pal::GOLD, -0.5f)}, {'G', Pal::GOLD}, {'W', shade(Pal::GOLD, 0.45f)}
  };
  drawSpriteMap(spr, COIN, 7, x - 3 * scale, y - 3 * scale, scale, l, 3);
}

void drawStarIcon(TFT_eSprite &spr, int x, int y, int scale, uint16_t color) {
  static const char *STAR[] = {
    "..#..",
    ".###.",
    "#####",
    ".###.",
    "#.#.#",
  };
  SpriteLegend legend[1] = { { '#', color } };
  drawSpriteMap(spr, STAR, 5, x, y, scale, legend, 1);
}

void drawLockIcon(TFT_eSprite &spr, int x, int y, int scale) {
  static const char *LOCK[] = {
    "..ooo..",
    ".o...o.",
    ".o...o.",
    "oGGGGGo",
    "oGGWGGo",
    "oGGGGGo",
    ".ooooo.",
  };
  SpriteLegend legend[3] = {
    {'o', shade(Pal::GOLD, -0.55f)}, {'G', Pal::GOLD}, {'W', Pal::PAPER}
  };
  drawSpriteMap(spr, LOCK, 7, x, y, scale, legend, 3);
}

// ---------------------------------------------------------------------------
// Playing cards
// ---------------------------------------------------------------------------
const int CARD_W = 22;
const int CARD_H = 30;

static const char *GLYPH_HEART[]   = { " # # ", "#####", "#####", " ### ", "  #  " };
static const char *GLYPH_DIAMOND[] = { "  #  ", " ### ", "#####", " ### ", "  #  " };
static const char *GLYPH_SPADE[]   = { "  #  ", " ### ", "#####", "  #  ", " ### " };
static const char *GLYPH_CLUB[]    = { " ### ", "#####", " ### ", "  #  ", " ### " };

static const char *rankLabel(int rank) {
  static char buf[3];
  if (rank == 1) return "A";
  if (rank == 11) return "J";
  if (rank == 12) return "Q";
  if (rank == 13) return "K";
  snprintf(buf, sizeof(buf), "%d", rank);
  return buf;
}

void drawCard(TFT_eSprite &spr, int x, int y, int rank, int suit, bool faceDown) {
  spr.fillRect(x + 2, y + 2, CARD_W, CARD_H, shade(Pal::BG, -0.3f));
  if (faceDown) {
    spr.fillRect(x, y, CARD_W, CARD_H, 0x2A6D);
    spr.drawRect(x, y, CARD_W, CARD_H, Pal::PAPER);
    spr.drawRect(x + 2, y + 2, CARD_W - 4, CARD_H - 4, Pal::GOLD);
    for (int py = 4; py < CARD_H - 4; py += 4)
      for (int px = 4; px < CARD_W - 4; px += 4)
        spr.drawPixel(x + px + ((py / 4) & 1) * 2, y + py, shade(0x2A6D, 0.45f));
    return;
  }
  spr.fillRect(x, y, CARD_W, CARD_H, 0xFF79);
  spr.drawRect(x, y, CARD_W, CARD_H, Pal::INK);
  spr.drawPixel(x, y, Pal::BG); spr.drawPixel(x + CARD_W - 1, y, Pal::BG);
  spr.drawPixel(x, y + CARD_H - 1, Pal::BG);
  spr.drawPixel(x + CARD_W - 1, y + CARD_H - 1, Pal::BG);

  bool red = (suit == 1 || suit == 2);
  uint16_t suitColor = red ? Pal::RED_ACCENT : Pal::INK;

  drawPixelText(spr, rankLabel(rank), x + 2, y + 2, 1, suitColor, suitColor);

  const char *const *glyph = suit == 0 ? GLYPH_SPADE : suit == 1 ? GLYPH_HEART : suit == 2 ? GLYPH_DIAMOND : GLYPH_CLUB;
  SpriteLegend legend[1] = { { '#', suitColor } };
  drawSpriteMap(spr, glyph, 5, x + (CARD_W - 5 * 2) / 2, y + CARD_H - 15, 2, legend, 1);
}

// ---------------------------------------------------------------------------
// Slot machine symbols
// ---------------------------------------------------------------------------
const int SLOT_PAYOUT[SLOT_SYMBOL_COUNT] = { 5, 7, 9, 12, 18, 30 };

static const char *SYM_CHERRY[] = {
  "..oo...oo..",
  ".o..o.o..o.",
  ".o RR o RR.",
  ".oRRRoRRRo.",
  "..oRo.oRo..",
};
static const char *SYM_LEMON[] = {
  "...ooo.....",
  "..oYYYo....",
  ".oYYYYYo...",
  ".oYYYYYo...",
  "..oYYYo....",
};
static const char *SYM_BELL[] = {
  "....o......",
  "...oYo.....",
  "..oYYYo....",
  ".oYYYYYo...",
  "ooooooooo..",
  "...ooo.....",
};
static const char *SYM_GOLD[] = {
  "..oooo.....",
  ".oGGGGo....",
  ".oGWGGo....",
  ".oGGGGo....",
  "..oooo.....",
};
static const char *SYM_SEVEN[] = {
  "ooooooo....",
  "......o....",
  ".....o.....",
  "....o......",
  "...o.......",
};
static const char *SYM_GEM[] = {
  "..ooooo....",
  ".oCCCCCo...",
  "oCCCCCCCo..",
  ".oCCCCCo...",
  "..oCoCo....",
};

void drawSlotSymbol(TFT_eSprite &spr, int x, int y, int scale, int symbolIndex) {
  symbolIndex = constrain(symbolIndex, 0, SLOT_SYMBOL_COUNT - 1);
  int pxScale = scale >= 3 ? 2 : 1;
  drawPremiumBitmap(spr, PREMIUM_SLOT_SYMBOLS[symbolIndex], PREMIUM_SLOT_W,
                    PREMIUM_SLOT_H, x, y, pxScale, false);
  return;
  uint16_t R = TFT_RED, Y = TFT_YELLOW, C = 0x2D9F, o = Pal::INK;
  switch (symbolIndex) {
    case 0: {
      SpriteLegend l[2] = { { 'o', o }, { 'R', R } };
      drawSpriteMap(spr, SYM_CHERRY, 5, x, y, scale, l, 2);
      break;
    }
    case 1: {
      SpriteLegend l[2] = { { 'o', o }, { 'Y', Y } };
      drawSpriteMap(spr, SYM_LEMON, 5, x, y, scale, l, 2);
      break;
    }
    case 2: {
      SpriteLegend l[2] = { { 'o', shade(Y, -0.4f) }, { 'Y', Y } };
      drawSpriteMap(spr, SYM_BELL, 6, x, y, scale, l, 2);
      break;
    }
    case 3: {
      SpriteLegend l[3] = { { 'o', shade(Pal::GOLD, -0.4f) }, { 'G', Pal::GOLD }, { 'W', shade(Pal::GOLD, 0.5f) } };
      drawSpriteMap(spr, SYM_GOLD, 5, x, y, scale, l, 3);
      break;
    }
    case 4: {
      SpriteLegend l[1] = { { 'o', R } };
      drawSpriteMap(spr, SYM_SEVEN, 5, x, y, scale, l, 1);
      break;
    }
    default: {
      SpriteLegend l[2] = { { 'o', shade(C, -0.3f) }, { 'C', C } };
      drawSpriteMap(spr, SYM_GEM, 5, x, y, scale, l, 2);
      break;
    }
  }
}
