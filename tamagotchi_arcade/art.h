#pragma once
#include "gfx.h"

// One companion: a cat. Its coat color is chosen once at character creation
// (see FUR_COLOR_* below) and is not a shop category - the shop instead
// goes deep on Backgrounds/Hats/Accessories/Patterns, all authored against
// this one fixed silhouette so every item is guaranteed to fit.
#define BACKGROUND_COUNT 10
#define HAT_COUNT 10
#define ACCESSORY_COUNT 10
#define PATTERN_COUNT 10

extern const char *BACKGROUND_NAMES[BACKGROUND_COUNT];
extern const char *HAT_NAMES[HAT_COUNT];
extern const char *ACCESSORY_NAMES[ACCESSORY_COUNT];
extern const char *PATTERN_NAMES[PATTERN_COUNT];

extern const int BACKGROUND_PRICES[BACKGROUND_COUNT];
extern const int HAT_PRICES[HAT_COUNT];
extern const int ACCESSORY_PRICES[ACCESSORY_COUNT];
extern const int PATTERN_PRICES[PATTERN_COUNT];

// Level required to even see/buy an item, separate from its coin price -
// this is what makes top-tier items read as "grind", not just "save up".
extern const int BACKGROUND_LEVELS[BACKGROUND_COUNT];
extern const int HAT_LEVELS[HAT_COUNT];
extern const int ACCESSORY_LEVELS[ACCESSORY_COUNT];
extern const int PATTERN_LEVELS[PATTERN_COUNT];

// Coat colors, picked once at character creation and fixed for the pet's
// lifetime (short of a full Reset Save). Not purchasable, not a shop
// category - just the initial "which cat is mine" choice.
#define FUR_COLOR_COUNT 10
extern const char *FUR_COLOR_NAMES[FUR_COLOR_COUNT];

enum PetExpression {
  EXPR_NEUTRAL,
  EXPR_HAPPY,
  EXPR_BLINK,
  EXPR_EXCITED,
  EXPR_SAD,
  EXPR_ASLEEP,
};

// Fills the whole sprite with the chosen background scene. dt is seconds
// since last frame, for ambient particle scenes (snow/leaves drift).
void drawBackground(TFT_eSprite &spr, int index, float dt);

// Draws the cat: coat (colored + patterned) with hat/accessory props, all
// anchored to this one fixed silhouette.
void drawPet(TFT_eSprite &spr, int cx, int cy, int scale, PetExpression expr,
             int furColorIndex, int patternIndex,
             int hatIndex, int accessoryIndex, int bobOffset);

// --- Small icons used throughout the UI (coin, star, lock) ---
void drawCoinIcon(TFT_eSprite &spr, int x, int y, int scale);
void drawStarIcon(TFT_eSprite &spr, int x, int y, int scale, uint16_t color);
void drawLockIcon(TFT_eSprite &spr, int x, int y, int scale);

// Standalone hat/accessory renders for shop preview thumbnails - same art
// used as overlays on the cat. anchorX/anchorY: bottom-center of the hat /
// top-center of the chest respectively; drawAccessory also takes a
// separate face anchor for glasses/sunglasses (eye level).
void drawHat(TFT_eSprite &spr, int anchorX, int anchorY, int scale, int hatIndex);
void drawAccessory(TFT_eSprite &spr, int chestX, int chestY, int faceX, int faceY,
                    int scale, int accIndex);

// One flat representative color per background scene, for small swatches
// where rendering the full scene would be too costly/cramped.
uint16_t backgroundSwatchColor(int index);
void drawBackgroundThumbnail(TFT_eSprite &spr, int x, int y, int index);

// --- Playing cards (Blackjack / High-Low) ---
// rank: 1-13 (1=Ace, 11=J, 12=Q, 13=K). suit: 0=Spade 1=Heart 2=Diamond 3=Club
void drawCard(TFT_eSprite &spr, int x, int y, int rank, int suit, bool faceDown);
extern const int CARD_W, CARD_H; // pixel footprint at scale 1

// --- Slot machine symbols (0..5) ---
void drawSlotSymbol(TFT_eSprite &spr, int x, int y, int scale, int symbolIndex);
#define SLOT_SYMBOL_COUNT 6
extern const int SLOT_PAYOUT[SLOT_SYMBOL_COUNT]; // per-symbol 3-match multiplier
