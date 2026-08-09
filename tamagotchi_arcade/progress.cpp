#include "progress.h"
#include "save.h"
#include "art.h"
#include "gfx.h"

const Achievement ACHIEVEMENTS[ACH_COUNT] = {
  { "First Win",     "Win any minigame" },
  { "Rising Star",   "Reach level 5" },
  { "High Roller",   "Reach level 10" },
  { "Nice Stash",    "Hold 500 coins" },
  { "Big Stash",     "Hold 2000 coins" },
  { "Hat Collector", "Own every hat" },
  { "World Traveler","Own every background" },
  { "Pattern Fan",   "Own 5 or more patterns" },
  { "Accessorized",  "Own every accessory" },
  { "Fashionista",   "Equip an item in every slot" },
  { "Pure Joy",      "Reach 100% happiness" },
  { "Dedicated",     "Play for 1 hour total" },
  { "Big Win",       "Win 100+ coins in one Blackjack hand" },
  { "Hot Streak",    "Hit a 5-streak in High-Low" },
  { "Jackpot!",      "Match 3 golds on the Slot Machine" },
  { "Perfect Memory","Reach round 10 in Simon Says" },
  { "Veteran",       "Reach level 20" },
  { "Pattern Master","Own every pattern" },
  { "Connoisseur",   "Own a premium item in every category" },
};

static bool allBitsSet(uint32_t mask, int count) {
  uint32_t full = (count >= 32) ? 0xFFFFFFFFUL : ((1UL << count) - 1);
  return (mask & full) == full;
}

static int popcount(uint32_t mask) {
  int n = 0;
  while (mask) { n += mask & 1; mask >>= 1; }
  return n;
}

void unlockAchievement(AchievementId id) {
  if (id < 0 || id >= ACH_COUNT) return;
  if (game.achievements & (1UL << id)) return; // already unlocked
  game.achievements |= (1UL << id);
  markSaveDirty();
  toastShow("Achievement!", ACHIEVEMENTS[id].name, Pal::GOLD);
}

bool hasAchievement(AchievementId id) {
  return (game.achievements & (1UL << id)) != 0;
}

int achievementCountUnlocked() {
  int n = 0;
  for (int i = 0; i < ACH_COUNT; i++) if (hasAchievement((AchievementId)i)) n++;
  return n;
}

void checkAchievements() {
  bool anyWin = false;
  for (int i = 0; i < NUM_MINIGAMES; i++) if (game.highScores[i] > 0) anyWin = true;
  if (anyWin) unlockAchievement(ACH_FIRST_WIN);

  if (game.level >= 5) unlockAchievement(ACH_LEVEL_5);
  if (game.level >= 10) unlockAchievement(ACH_LEVEL_10);
  if (game.coins >= 500) unlockAchievement(ACH_RICH_500);
  if (game.coins >= 2000) unlockAchievement(ACH_RICH_2000);

  if (allBitsSet(game.owned[CAT_HAT], HAT_COUNT)) unlockAchievement(ACH_ALL_HATS);
  if (allBitsSet(game.owned[CAT_BACKGROUND], BACKGROUND_COUNT)) unlockAchievement(ACH_ALL_BACKGROUNDS);
  if (popcount(game.owned[CAT_PATTERN]) >= 5) unlockAchievement(ACH_HALF_PATTERNS);
  if (allBitsSet(game.owned[CAT_ACCESSORY], ACCESSORY_COUNT)) unlockAchievement(ACH_ALL_ACCESSORIES);

  bool fashionista = true;
  for (int i = 0; i < CAT_COUNT; i++) if (game.equipped[i] == 0) fashionista = false;
  if (fashionista) unlockAchievement(ACH_FASHIONISTA);

  if (game.happiness >= 100) unlockAchievement(ACH_HAPPY_100);
  if (game.totalPlaySeconds >= 3600) unlockAchievement(ACH_DEDICATED);

  if (game.level >= 20) unlockAchievement(ACH_LEVEL_20);
  if (allBitsSet(game.owned[CAT_PATTERN], PATTERN_COUNT)) unlockAchievement(ACH_ALL_PATTERNS);

  bool premiumInEvery = true;
  for (int i = 0; i < CAT_COUNT; i++) {
    if ((game.owned[i] >> 7) == 0) { premiumInEvery = false; break; } // no item index 7+ owned
  }
  if (premiumInEvery) unlockAchievement(ACH_PREMIUM_COLLECTOR);
}
