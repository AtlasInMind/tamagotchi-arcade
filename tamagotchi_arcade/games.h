#pragma once
#include "input.h"
#include "save.h" // NUM_MINIGAMES

// Order must match the highScores[] slots in save.h (NUM_MINIGAMES).
enum GameID {
  GAME_ID_BLACKJACK,
  GAME_ID_HIGHLOW,
  GAME_ID_TIMING,
  GAME_ID_SLOT,
  GAME_ID_SIMON,
};

extern const char *GAME_NAMES[NUM_MINIGAMES];

void gameBegin(GameID id);
void gameTick(); // called every loop iteration for animation timing
// Returns true once the game wants to exit back to the arcade list.
bool gameHandleEvent(ButtonEvent evt);
void gameRender();

// Only meaningful while the High-Low game is active; called from the
// extended menu's "Cash Out" action.
void highLowCashOut();
