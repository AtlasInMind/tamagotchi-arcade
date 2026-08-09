#pragma once

enum AchievementId {
  ACH_FIRST_WIN,
  ACH_LEVEL_5,
  ACH_LEVEL_10,
  ACH_RICH_500,
  ACH_RICH_2000,
  ACH_ALL_HATS,
  ACH_ALL_BACKGROUNDS,
  ACH_HALF_PATTERNS, // own 5+ patterns (was "own every color" before coat color became a one-time creation choice)
  ACH_ALL_ACCESSORIES,
  ACH_FASHIONISTA,     // something equipped in every category at once
  ACH_HAPPY_100,
  ACH_DEDICATED,        // 1 hour of cumulative playtime
  ACH_BIG_WIN,          // a single Blackjack payout of 100+ coins
  ACH_HIGHLOW_STREAK5,  // explicit unlock from games.cpp
  ACH_SLOT_JACKPOT,     // explicit unlock from games.cpp
  ACH_SIMON_ROUND10,    // explicit unlock from games.cpp

  // --- v3 additions: tied to the deeper item catalog and grind curve ---
  ACH_LEVEL_20,          // the level the most expensive items require
  ACH_ALL_PATTERNS,
  ACH_PREMIUM_COLLECTOR, // own a top-tier (index 7+) item in every category
  ACH_COUNT
};

struct Achievement { const char *name; const char *desc; };
extern const Achievement ACHIEVEMENTS[ACH_COUNT];

// Re-checks every achievement derivable purely from persistent GameData
// (level, coins, happiness, ownership, playtime) and unlocks + toasts any
// newly-earned ones. Cheap - safe to call after any state-changing action.
void checkAchievements();

// For achievements only the minigame logic can observe in the moment
// (a specific streak, a specific reel result). No-op if already unlocked.
void unlockAchievement(AchievementId id);

bool hasAchievement(AchievementId id);
int achievementCountUnlocked();
