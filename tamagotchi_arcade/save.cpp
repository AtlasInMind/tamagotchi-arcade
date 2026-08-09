#include <Preferences.h>
#include "save.h"

static Preferences prefs;
static const char *NS = "tama";

GameData game;

static void applyDefaults() {
  game.coins = 50;
  game.xp = 0;
  game.level = 1;
  for (int i = 0; i < CAT_COUNT; i++) {
    game.owned[i] = 1; // index 0 in every category ("None"/default) is free and pre-owned
    game.equipped[i] = 0;
  }
  game.happiness = 70;
  game.totalPlaySeconds = 0;
  for (int i = 0; i < NUM_MINIGAMES; i++) game.highScores[i] = 0;
  game.achievements = 0;
  game.created = false; // brand-new save: character creation should run
  game.furColor = 0;
}

void saveBegin() {
  prefs.begin(NS, false);
  if (!prefs.getBool("init", false)) {
    applyDefaults();
    saveNow();
    prefs.putBool("init", true);
    prefs.putUChar("ver", SAVE_SCHEMA_VERSION);
    return;
  }

  game.coins = prefs.getUInt("coins", 50);
  game.xp = prefs.getUInt("xp", 0);
  game.level = prefs.getUShort("level", 1);
  game.happiness = prefs.getUChar("happy", 70);
  game.totalPlaySeconds = prefs.getUInt("playsec", 0);

  char key[8];
  for (int i = 0; i < CAT_COUNT; i++) {
    snprintf(key, sizeof(key), "own%d", i);
    game.owned[i] = prefs.getUInt(key, 0);
    snprintf(key, sizeof(key), "eq%d", i);
    game.equipped[i] = (int8_t)prefs.getChar(key, 0);
  }

  // v2 fields: on a save written by v1, these keys simply don't exist yet and
  // getUInt/getUInt fall back to 0 - no explicit migration step needed, and
  // v1 coins/XP/unlocks are untouched.
  for (int i = 0; i < NUM_MINIGAMES; i++) {
    snprintf(key, sizeof(key), "hs%d", i);
    game.highScores[i] = prefs.getUInt(key, 0);
  }
  game.achievements = prefs.getUInt("ach", 0);

  // v3 field. `created` defaults to true here specifically because reaching
  // this branch means "init" was already true - i.e. this is an upgrade
  // from a save that predates character creation, so treat it as already
  // created rather than forcing creation on an existing pet with existing
  // progress.
  game.created = prefs.getBool("created", true);
  game.furColor = prefs.getUChar("furcolor", 0);

  uint8_t savedVer = prefs.getUChar("ver", 1);
  if (savedVer < 4) {
    // v4 removed the CAT_COLOR category and shifted every category after it
    // down by one index, so own2/eq2 (used to be COLOR) would otherwise be
    // misread as the new index 2 (ACCESSORY). Rather than trust misaligned
    // data, reset ownership/equipped state to safe defaults on this one-time
    // upgrade - coins/XP/level/achievements/high scores are untouched.
    for (int i = 0; i < CAT_COUNT; i++) {
      game.owned[i] = 1;
      game.equipped[i] = 0;
    }
  }
  if (savedVer < SAVE_SCHEMA_VERSION) {
    prefs.putUChar("ver", SAVE_SCHEMA_VERSION);
    saveNow(); // persist the new keys/realigned categories immediately
  }
}

void saveNow() {
  prefs.putUInt("coins", game.coins);
  prefs.putUInt("xp", game.xp);
  prefs.putUShort("level", game.level);
  prefs.putUChar("happy", game.happiness);
  prefs.putUInt("playsec", game.totalPlaySeconds);

  char key[8];
  for (int i = 0; i < CAT_COUNT; i++) {
    snprintf(key, sizeof(key), "own%d", i);
    prefs.putUInt(key, game.owned[i]);
    snprintf(key, sizeof(key), "eq%d", i);
    prefs.putChar(key, game.equipped[i]);
  }
  for (int i = 0; i < NUM_MINIGAMES; i++) {
    snprintf(key, sizeof(key), "hs%d", i);
    prefs.putUInt(key, game.highScores[i]);
  }
  prefs.putUInt("ach", game.achievements);
  prefs.putBool("created", game.created);
  prefs.putUChar("furcolor", game.furColor);
}

static bool dirty = false;
static unsigned long lastFlushMs = 0;
static const unsigned long SAVE_DEBOUNCE_MS = 3000;

void markSaveDirty() {
  dirty = true;
}

void saveTick() {
  if (!dirty || millis() - lastFlushMs < SAVE_DEBOUNCE_MS) return;
  saveNow();
  dirty = false;
  lastFlushMs = millis();
}

void saveFlushNow() {
  if (!dirty) return;
  saveNow();
  dirty = false;
  lastFlushMs = millis();
}

void saveResetProgress() {
  applyDefaults(); // sets created=false, so this also re-triggers character creation
  saveNow();
  prefs.putUChar("ver", SAVE_SCHEMA_VERSION);
}

void markCreated(uint8_t furColor) {
  game.furColor = furColor;
  game.created = true;
  saveNow();
}

int xpForNextLevel(int level) {
  return 50 + (level - 1) * 30; // gentle linear ramp
}

void addXP(uint32_t amount) {
  game.xp += amount;
  int need = xpForNextLevel(game.level);
  while ((int)game.xp >= need) {
    game.xp -= need;
    game.level++;
    need = xpForNextLevel(game.level);
  }
}

void addCoins(int32_t amount) {
  if (amount < 0 && (uint32_t)(-amount) > game.coins) {
    game.coins = 0;
  } else {
    game.coins += amount;
  }
}

void bumpHappiness(int amount) {
  int h = (int)game.happiness + amount;
  if (h < 0) h = 0;
  if (h > 100) h = 100;
  game.happiness = (uint8_t)h;
}

bool isOwned(ItemCategory cat, int itemIndex) {
  return (game.owned[cat] & (1UL << itemIndex)) != 0;
}

void setOwned(ItemCategory cat, int itemIndex) {
  game.owned[cat] |= (1UL << itemIndex);
}

bool reportHighScore(int gameIndex, uint32_t score) {
  if (gameIndex < 0 || gameIndex >= NUM_MINIGAMES) return false;
  if (score > game.highScores[gameIndex]) {
    game.highScores[gameIndex] = score;
    return true;
  }
  return false;
}
