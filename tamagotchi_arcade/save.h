#pragma once
#include <Arduino.h>

// Coat color is NOT here - it's chosen once at character creation and
// stored directly as `furColor` below, not as an owned/equipped shop
// category (see art.h's FUR_COLOR_* and the v3->v4 migration note in
// saveBegin()).
enum ItemCategory {
  CAT_BACKGROUND,
  CAT_HAT,
  CAT_ACCESSORY,
  CAT_PATTERN,
  CAT_COUNT
};

// Must match GameID order in games.h - kept here (rather than including
// games.h) so save.h has no dependency on the gameplay layer.
#define NUM_MINIGAMES 5

#define SAVE_SCHEMA_VERSION 4

struct GameData {
  uint32_t coins;
  uint32_t xp;
  uint16_t level;
  uint32_t owned[CAT_COUNT];       // bitmask, bit i = item i owned
  int8_t equipped[CAT_COUNT];      // item index, 0 = default/none
  uint8_t happiness;                // 0-100
  uint32_t totalPlaySeconds;        // cumulative runtime across reboots

  // --- v2 additions. Reading these via Preferences with sane defaults on a
  // save written by v1 costs nothing extra - see saveBegin(). ---
  uint32_t highScores[NUM_MINIGAMES];
  uint32_t achievements;            // bitmask, see progress.h for bit meanings

  // --- v3 additions: character creation. `created` gates whether the
  // creation flow runs; see saveBegin() for how existing saves are treated
  // as already-created rather than forced through creation retroactively. ---
  bool created;

  // --- v4: collapsed to a single companion (a cat). Coat color is chosen
  // once at creation and fixed thereafter - it replaced the old per-species
  // selection and the old CAT_COLOR shop category. Saves from v3 (which had
  // a speciesIndex and CAT_COLOR-owned colors) simply get a default
  // furColor; nothing about their coins/XP/other unlocks is touched.
  uint8_t furColor;
};

extern GameData game;

void saveBegin();          // loads from NVS (transparently upgrades older saves) or initializes defaults
void saveNow();             // writes current `game` to NVS unconditionally
void saveResetProgress();   // wipes save, resets `game` to defaults (created=false), writes it
void markCreated(uint8_t furColor); // completes character creation, persists immediately

// Debounced persistence: gameplay code should call markSaveDirty() after
// mutating `game` instead of saveNow() directly. saveTick() (called once per
// loop() iteration) flushes to NVS on a timer so rapid-fire actions (e.g.
// mashing the slot machine) collapse into one write instead of one per
// action. saveFlushNow() bypasses the timer for deliberate checkpoints
// (quitting a game, going idle) without forcing a write if nothing changed.
void markSaveDirty();
void saveTick();
void saveFlushNow();

int xpForNextLevel(int level);
void addXP(uint32_t amount);
void addCoins(int32_t amount);     // negative allowed for purchases; clamps at 0
void bumpHappiness(int amount);     // clamps to 0-100
bool isOwned(ItemCategory cat, int itemIndex);
void setOwned(ItemCategory cat, int itemIndex);
bool reportHighScore(int gameIndex, uint32_t score); // returns true if it's a new record
