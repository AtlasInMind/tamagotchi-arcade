#include <Preferences.h>
#include <nvs_flash.h>
#include "save.h"
#include "gfx.h"
#include "art.h"

static Preferences prefs;
static const char *NS = "tama";

// False if NVS could not be opened even after one erase+retry (see
// saveBegin()) - the game then runs entirely in-memory for the session
// instead of silently discarding every write. saveNow() is the sole
// guard point; saveTick()/saveFlushNow() call through it.
static bool saveAvailable = true;

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

// CRC32 (standard reflected polynomial) over exactly the fields saveNow()
// persists, so a load can detect that a value was corrupted rather than
// trusting whatever bytes NVS returned. Bit-by-bit rather than table-driven
// since the dataset is ~60 bytes and this runs at most once per save/load.
static uint32_t computeCrc() {
  uint32_t crc = 0xFFFFFFFF;
  auto feed = [&](const void *p, size_t n) {
    const uint8_t *b = (const uint8_t *)p;
    for (size_t i = 0; i < n; i++) {
      crc ^= b[i];
      for (int bit = 0; bit < 8; bit++)
        crc = (crc >> 1) ^ (0xEDB88320 & (~(crc & 1) + 1));
    }
  };
  feed(&game.coins, sizeof(game.coins));
  feed(&game.xp, sizeof(game.xp));
  feed(&game.level, sizeof(game.level));
  feed(game.owned, sizeof(game.owned));
  feed(game.equipped, sizeof(game.equipped));
  feed(&game.happiness, sizeof(game.happiness));
  feed(&game.totalPlaySeconds, sizeof(game.totalPlaySeconds));
  feed(game.highScores, sizeof(game.highScores));
  feed(&game.achievements, sizeof(game.achievements));
  feed(&game.created, sizeof(game.created));
  feed(&game.furColor, sizeof(game.furColor));
  return crc ^ 0xFFFFFFFF;
}

static int categoryCount(int cat) {
  switch (cat) {
    case CAT_BACKGROUND: return BACKGROUND_COUNT;
    case CAT_HAT: return HAT_COUNT;
    case CAT_ACCESSORY: return ACCESSORY_COUNT;
    case CAT_PATTERN: return PATTERN_COUNT;
    default: return 1;
  }
}

// Defense-in-depth independent of the CRC check above: even data that
// passes CRC (or predates it) gets every field clamped into the range its
// consumers expect, so a future render-path change can never regress into
// trusting an unchecked index again. Selector fields (equipped/furColor)
// reset to their "None"/default value, matching the existing v4-migration
// convention below; magnitude fields (level/happiness) clamp to their
// boundary, matching bumpHappiness()'s existing style.
static void clampLoadedData() {
  if (game.level < 1) game.level = 1;
  if (game.level > 999) game.level = 999;
  if (game.happiness > 100) game.happiness = 100;
  if (game.furColor >= FUR_COLOR_COUNT) game.furColor = 0;
  for (int i = 0; i < CAT_COUNT; i++) {
    int count = categoryCount(i);
    if (game.equipped[i] < 0 || game.equipped[i] >= count) game.equipped[i] = 0;
    uint32_t validBits = (count >= 32) ? 0xFFFFFFFFu : ((1UL << count) - 1);
    game.owned[i] &= validBits;
  }
}

void saveBegin() {
  saveAvailable = prefs.begin(NS, false);
  if (!saveAvailable) {
    // Most common real-world cause: NVS partition never formatted, or
    // corrupted by a partition-table change. One erase+reinit retry
    // recovers from both.
    Serial.println("NVS begin() failed, erasing and retrying once...");
    nvs_flash_erase();
    nvs_flash_init();
    saveAvailable = prefs.begin(NS, false);
  }
  if (!saveAvailable) {
    Serial.println("NVS unavailable - progress will not be saved this session.");
    applyDefaults();
    toastShow("Save unavailable", "Progress won't persist", Pal::RED_ACCENT);
    return; // game.created stays false -> normal character-creation flow still runs
  }

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

  // Saves written before this check existed have no "crc" key at all -
  // isKey() lets us skip the check for those instead of wrongly flagging
  // every pre-existing save as corrupted. A key that exists and mismatches
  // means the bytes actually changed since they were written.
  if (prefs.isKey("crc") && prefs.getUInt("crc", 0) != computeCrc()) {
    Serial.println("Save data failed CRC check - resetting to defaults.");
    applyDefaults();
    toastShow("Save corrupted", "Progress was reset", Pal::RED_ACCENT);
    saveNow();
    prefs.putUChar("ver", SAVE_SCHEMA_VERSION);
    return;
  }

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
  clampLoadedData();
  if (savedVer < SAVE_SCHEMA_VERSION) {
    prefs.putUChar("ver", SAVE_SCHEMA_VERSION);
    saveNow(); // persist the new keys/realigned categories immediately
  }
}

void saveNow() {
  if (!saveAvailable) return;
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
  prefs.putUInt("crc", computeCrc());
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
