#include <Arduino.h>
#include <string.h>
#include "games.h"
#include "ui.h"
#include "save.h"
#include "art.h"
#include "gfx.h"
#include "progress.h"

const char *GAME_NAMES[NUM_MINIGAMES] = { "Blackjack", "High-Low", "Timing Bar", "Slot Machine", "Simon Says" };

static GameID activeGame;

// ---------------------------------------------------------------------------
// shared card helpers (Blackjack + High-Low)
// ---------------------------------------------------------------------------
static int drawRank() { return 1 + random(13); }  // 1..13
static int drawSuit() { return random(4); }        // 0..3
static int cardBjValue(int v) { return v >= 10 ? 10 : (v == 1 ? 11 : v); }

static int handTotal(int *ranks, int n) {
  int total = 0, aces = 0;
  for (int i = 0; i < n; i++) {
    total += cardBjValue(ranks[i]);
    if (ranks[i] == 1) aces++;
  }
  while (total > 21 && aces > 0) { total -= 10; aces--; }
  return total;
}

// ---------------------------------------------------------------------------
// Blackjack
// ---------------------------------------------------------------------------
enum BJState { BJ_BET, BJ_PLAYING, BJ_RESULT };
static BJState bjState;
static int bjPlayerRank[8], bjPlayerSuit[8], bjPlayerN;
static int bjDealerRank[8], bjDealerSuit[8], bjDealerN;
static int bjWager;
static const int WAGER_OPTIONS[] = { 5, 10, 20, 50, 100, 200 };
static const int WAGER_COUNT = 6;
static int bjWagerIdx;
static char bjResultMsg[32];

static void bjBegin() { bjState = BJ_BET; bjWagerIdx = 0; }

static void bjDraw(int *ranks, int *suits, int &n) {
  ranks[n] = drawRank();
  suits[n] = drawSuit();
  n++;
}

static void bjStartRound() {
  bjPlayerN = bjDealerN = 0;
  bjDraw(bjPlayerRank, bjPlayerSuit, bjPlayerN);
  bjDraw(bjPlayerRank, bjPlayerSuit, bjPlayerN);
  bjDraw(bjDealerRank, bjDealerSuit, bjDealerN);
  bjDraw(bjDealerRank, bjDealerSuit, bjDealerN);
  bjWager = WAGER_OPTIONS[bjWagerIdx];
  addCoins(-bjWager);
  bjState = BJ_PLAYING;
}

static void bjSettle() {
  while (handTotal(bjDealerRank, bjDealerN) < 17 && bjDealerN < 8) bjDraw(bjDealerRank, bjDealerSuit, bjDealerN);

  int p = handTotal(bjPlayerRank, bjPlayerN);
  int d = handTotal(bjDealerRank, bjDealerN);
  bool playerBust = p > 21;
  bool dealerBust = d > 21;

  addXP(10);
  bumpHappiness(5);
  int winnings = 0;
  if (playerBust) {
    snprintf(bjResultMsg, sizeof(bjResultMsg), "BUST! -%dc", bjWager);
  } else if (dealerBust || p > d) {
    winnings = bjWager * 2;
    addCoins(winnings);
    addXP(15);
    snprintf(bjResultMsg, sizeof(bjResultMsg), "WIN! +%dc", winnings);
    particlesSpawnBurst(SCREEN_W / 2, SCREEN_H / 2, Pal::GOLD, 10);
  } else if (p == d) {
    addCoins(bjWager);
    snprintf(bjResultMsg, sizeof(bjResultMsg), "PUSH");
  } else {
    snprintf(bjResultMsg, sizeof(bjResultMsg), "LOSE -%dc", bjWager);
  }
  if (winnings > 0) {
    reportHighScore(GAME_ID_BLACKJACK, winnings);
    if (winnings >= 100) unlockAchievement(ACH_BIG_WIN);
  }
  markSaveDirty();
  checkAchievements();
  bjState = BJ_RESULT;
}

static bool bjHandleEvent(ButtonEvent evt) {
  switch (bjState) {
    case BJ_BET:
      if (evt == EVT_L_PRESS) bjWagerIdx = (bjWagerIdx + 1) % WAGER_COUNT;
      else if (evt == EVT_R_PRESS) {
        if ((uint32_t)WAGER_OPTIONS[bjWagerIdx] <= game.coins) bjStartRound();
        else toastShow("Not enough coins", nullptr, Pal::RED_ACCENT);
      }
      break;
    case BJ_PLAYING:
      if (evt == EVT_L_PRESS) {
        bjDraw(bjPlayerRank, bjPlayerSuit, bjPlayerN);
        if (handTotal(bjPlayerRank, bjPlayerN) > 21 || bjPlayerN >= 8) bjSettle();
      } else if (evt == EVT_R_PRESS) {
        bjSettle();
      }
      break;
    case BJ_RESULT:
      if (evt == EVT_R_PRESS) bjState = BJ_BET;
      break;
  }
  return false;
}

static void bjRenderHand(int *ranks, int *suits, int n, int y, bool hideSecond) {
  int cardW = CARD_W + 4;
  int totalW = n * cardW - 4;
  int x = (SCREEN_W - totalW) / 2;
  if (x < 2) { cardW = (SCREEN_W - 4) / n; x = 2; }
  for (int i = 0; i < n; i++) {
    bool faceDown = hideSecond && i == 1;
    drawCard(uiSprite(), x + i * cardW, y, ranks[i], suits[i], faceDown);
  }
}

static void bjRender() {
  TFT_eSprite &s = uiSprite();
  drawScreenTitle("Blackjack");

  if (bjState == BJ_BET) {
    char buf[24];
    snprintf(buf, sizeof(buf), "Wager: %dc", WAGER_OPTIONS[bjWagerIdx]);
    drawCenteredMessage(buf, "R to deal");
    drawHintBar("Change", "Deal");
    return;
  }

  bool hideSecond = (bjState == BJ_PLAYING);
  drawPixelTextC(s, "DEALER", SCREEN_W / 2, UI_CONTENT_Y + 24, 1, shade(Pal::PAPER, -0.2f), Pal::INK);
  bjRenderHand(bjDealerRank, bjDealerSuit, bjDealerN, UI_CONTENT_Y + 36, hideSecond);
  if (!hideSecond) {
    char buf[16];
    snprintf(buf, sizeof(buf), "= %d", handTotal(bjDealerRank, bjDealerN));
    drawPixelTextC(s, buf, SCREEN_W / 2, UI_CONTENT_Y + 70, 1, Pal::PAPER, Pal::INK);
  }

  drawPixelTextC(s, "YOU", SCREEN_W / 2, UI_CONTENT_Y + 96, 1, shade(Pal::PAPER, -0.2f), Pal::INK);
  bjRenderHand(bjPlayerRank, bjPlayerSuit, bjPlayerN, UI_CONTENT_Y + 108, false);
  char buf[16];
  snprintf(buf, sizeof(buf), "= %d", handTotal(bjPlayerRank, bjPlayerN));
  drawPixelTextC(s, buf, SCREEN_W / 2, UI_CONTENT_Y + 142, 1, Pal::PAPER, Pal::INK);

  if (bjState == BJ_PLAYING) {
    drawHintBar("Hit", "Stand");
  } else {
    drawPixelTextC(s, bjResultMsg, SCREEN_W / 2, UI_CONTENT_Y + 160, 2, Pal::GOLD, Pal::INK);
    drawHintBar("-", "Again");
  }
}

// ---------------------------------------------------------------------------
// High-Low
// ---------------------------------------------------------------------------
static int hlRank, hlSuit;
static int hlPot, hlStreak;
static char hlMsg[24];
static bool hlShowMsg;
static unsigned long hlMsgUntil;

static void hlBegin() {
  hlRank = drawRank();
  hlSuit = drawSuit();
  hlPot = 0;
  hlStreak = 0;
  hlShowMsg = false;
}

void highLowCashOut() {
  if (hlPot > 0) {
    addCoins(hlPot);
    reportHighScore(GAME_ID_HIGHLOW, hlStreak);
    snprintf(hlMsg, sizeof(hlMsg), "Cashed out %dc!", hlPot);
    hlPot = 0;
    hlStreak = 0;
    hlShowMsg = true;
    hlMsgUntil = millis() + 900;
    markSaveDirty();
  }
}

static bool hlHandleEvent(ButtonEvent evt) {
  if (evt != EVT_L_PRESS && evt != EVT_R_PRESS) return false;
  bool guessHigher = (evt == EVT_R_PRESS);
  int nextRank = drawRank();
  int nextSuit = drawSuit();

  if (nextRank == hlRank) {
    strcpy(hlMsg, "PUSH - same rank");
  } else {
    bool correct = guessHigher ? (nextRank > hlRank) : (nextRank < hlRank);
    if (correct) {
      hlStreak++;
      hlPot += 5 * hlStreak;
      addXP(5);
      bumpHappiness(5);
      if (hlStreak >= 5) unlockAchievement(ACH_HIGHLOW_STREAK5);
      snprintf(hlMsg, sizeof(hlMsg), "Correct! Pot: %dc", hlPot);
    } else {
      addXP(5);
      bumpHappiness(5);
      reportHighScore(GAME_ID_HIGHLOW, hlStreak);
      snprintf(hlMsg, sizeof(hlMsg), "Wrong! Lost %dc", hlPot);
      hlStreak = 0;
      hlPot = 0;
    }
  }
  hlRank = nextRank;
  hlSuit = nextSuit;
  hlShowMsg = true;
  hlMsgUntil = millis() + 900;
  markSaveDirty();
  checkAchievements();
  return false;
}

static void hlRender() {
  TFT_eSprite &s = uiSprite();
  drawScreenTitle("High-Low");

  drawCard(s, SCREEN_W / 2 - CARD_W / 2, UI_CONTENT_Y + 40, hlRank, hlSuit, false);

  char buf[24];
  snprintf(buf, sizeof(buf), "Streak %d", hlStreak);
  drawPixelTextC(s, buf, SCREEN_W / 2, UI_CONTENT_Y + 90, 1, Pal::PAPER, Pal::INK);
  snprintf(buf, sizeof(buf), "Pot: %dc", hlPot);
  drawPixelTextC(s, buf, SCREEN_W / 2, UI_CONTENT_Y + 104, 2, Pal::GOLD, Pal::INK);

  if (hlShowMsg && millis() < hlMsgUntil) {
    drawPixelTextC(s, hlMsg, SCREEN_W / 2, UI_CONTENT_Y + 130, 1, shade(Pal::PAPER, -0.2f), Pal::INK);
  }
  drawPixelTextC(s, "Hold L: Cash Out", SCREEN_W / 2, UI_CONTENT_Y + 155, 1, shade(Pal::PAPER, -0.4f), Pal::INK);
  drawHintBar("Lower", "Higher");
}

// ---------------------------------------------------------------------------
// Timing Bar (vertical, portrait-native)
// ---------------------------------------------------------------------------
enum TBState { TB_RUNNING, TB_RESULT };
static TBState tbState;
static float tbPos;
static float tbSpeed;
static unsigned long tbLastMs;
static int tbTargetCenter, tbTargetHalfWidth;
static char tbMsg[24];
static int tbTrackTop, tbTrackBottom;
static bool tbSuppressNextRelease = false;

static void tbBegin() {
  tbState = TB_RUNNING;
  tbSuppressNextRelease = false;
  tbTrackTop = UI_CONTENT_Y + 20;
  // Leave enough room below the track for the size-1 result message (~8px
  // tall) drawn at tbTrackBottom + 6 in tbRender().
  tbTrackBottom = SCREEN_H - UI_BOTTOM_H - 20;
  tbPos = tbTrackTop;
  tbSpeed = 90.0f + game.level * 4.0f;
  tbLastMs = millis();
  tbTargetCenter = tbTrackTop + 20 + random(tbTrackBottom - tbTrackTop - 40);
  tbTargetHalfWidth = 16;
}

static bool tbHandleEvent(ButtonEvent evt) {
  // Stop on the instant the button is pressed, not on release - EVT_R_DOWN
  // fires there, unlike every other screen's release-triggered EVT_R_PRESS.
  // The matching release still fires EVT_R_PRESS afterward; suppress that
  // one so it doesn't also get read as "Again" and restart the round.
  if (tbState == TB_RUNNING && evt == EVT_R_DOWN) {
    int dist = abs((int)tbPos - tbTargetCenter);
    int coins = 0;
    const char *rank;
    if (dist <= 3) { coins = 30; rank = "PERFECT!"; }
    else if (dist <= 9) { coins = 15; rank = "GREAT!"; }
    else if (dist <= tbTargetHalfWidth) { coins = 5; rank = "OK"; }
    else { coins = 0; rank = "MISS"; }

    addCoins(coins);
    addXP(coins > 0 ? 12 : 8);
    bumpHappiness(5);
    if (coins > 0) {
      reportHighScore(GAME_ID_TIMING, coins);
      particlesSpawnBurst(SCREEN_W / 2, tbTargetCenter, Pal::GOLD, coins / 3);
    }
    snprintf(tbMsg, sizeof(tbMsg), "%s +%dc", rank, coins);
    markSaveDirty();
    checkAchievements();
    tbSuppressNextRelease = true;
    tbState = TB_RESULT;
  } else if (tbState == TB_RESULT && evt == EVT_R_PRESS) {
    if (tbSuppressNextRelease) { tbSuppressNextRelease = false; return false; }
    tbBegin();
  } else if (evt == EVT_R_HOLD) {
    // A stop-press held past HOLD_MS never generates the matching release
    // event at all (input.cpp withholds it once a hold has fired), so a
    // pending suppression would otherwise never get cleared.
    tbSuppressNextRelease = false;
  }
  return false;
}

static void tbTick() {
  if (tbState != TB_RUNNING) return;
  unsigned long now = millis();
  float dt = (now - tbLastMs) / 1000.0f;
  tbLastMs = now;
  tbPos += tbSpeed * dt;
  if (tbPos < tbTrackTop || tbPos > tbTrackBottom) {
    tbSpeed = -tbSpeed;
    tbPos = constrain(tbPos, (float)tbTrackTop, (float)tbTrackBottom);
  }
}

static void tbRender() {
  TFT_eSprite &s = uiSprite();
  drawScreenTitle("Timing Bar");

  int trackX = SCREEN_W / 2;
  s.drawFastVLine(trackX - 10, tbTrackTop, tbTrackBottom - tbTrackTop, Pal::PANEL_DARK);
  s.drawFastVLine(trackX + 10, tbTrackTop, tbTrackBottom - tbTrackTop, Pal::PANEL_DARK);
  s.fillRect(trackX - 12, tbTargetCenter - tbTargetHalfWidth, 24, tbTargetHalfWidth * 2, Pal::GREEN_ACCENT);
  s.fillRect(trackX - 12, tbTargetCenter - 3, 24, 6, shade(Pal::GREEN_ACCENT, 0.4f));
  s.fillRect(trackX - 16, (int)tbPos - 2, 32, 4, Pal::GOLD);

  if (tbState == TB_RUNNING) {
    drawHintBar("-", "Stop");
  } else {
    drawPixelTextC(s, tbMsg, SCREEN_W / 2, tbTrackBottom + 6, 1, Pal::GOLD, Pal::INK);
    drawHintBar("-", "Again");
  }
}

// ---------------------------------------------------------------------------
// Slot Machine
// ---------------------------------------------------------------------------
enum SlotState { SLOT_MODE_SELECT, SLOT_IDLE, SLOT_SPINNING, SLOT_RESULT };
static SlotState slotState;
static bool reelStopped[3];
static int reelSymbol[3];
static float reelOffset[3]; // for spin blur animation
static unsigned long slotLastMs;
static unsigned long slotSpinStarted;
static bool slotAutoMode;
static const int SLOT_WAGER = 10;
static char slotMsg[24];

static void slotBegin() {
  slotState = SLOT_MODE_SELECT;
  slotAutoMode = false;
  for (int i = 0; i < 3; i++) { reelStopped[i] = true; reelSymbol[i] = random(SLOT_SYMBOL_COUNT); reelOffset[i] = 0; }
  slotMsg[0] = '\0';
}

static void slotSpin() {
  if (game.coins < (uint32_t)SLOT_WAGER) {
    toastShow("Not enough coins", nullptr, Pal::RED_ACCENT);
    return;
  }
  addCoins(-SLOT_WAGER);
  slotState = SLOT_SPINNING;
  for (int i = 0; i < 3; i++) { reelStopped[i] = false; reelOffset[i] = 0; }
  slotLastMs = millis();
  slotSpinStarted = slotLastMs;
}

static void slotSettle() {
  int a = reelSymbol[0], b = reelSymbol[1], c = reelSymbol[2];
  int winnings = 0;
  if (a == b && b == c) {
    winnings = SLOT_WAGER * SLOT_PAYOUT[a];
    if (!slotAutoMode) winnings += 10;
    snprintf(slotMsg, sizeof(slotMsg), "JACKPOT! +%dc", winnings);
    if (a == 3) unlockAchievement(ACH_SLOT_JACKPOT); // triple gold
    particlesSpawnBurst(SCREEN_W / 2, UI_CONTENT_Y + 90, Pal::GOLD, 14);
  } else if (a == b || b == c || a == c) {
    int pairSymbol = (a == b) ? a : (b == c) ? b : c;
    winnings = 20 + pairSymbol * 6 + (!slotAutoMode ? 5 : 0);
    snprintf(slotMsg, sizeof(slotMsg), "Pair! +%dc", winnings);
  } else {
    snprintf(slotMsg, sizeof(slotMsg), "No match");
  }
  if (winnings > 0) {
    addCoins(winnings);
    reportHighScore(GAME_ID_SLOT, winnings);
  }
  addXP(8);
  bumpHappiness(5);
  markSaveDirty();
  checkAchievements();
  slotState = SLOT_RESULT;
}

static bool slotHandleEvent(ButtonEvent evt) {
  if (slotState == SLOT_MODE_SELECT) {
    if (evt == EVT_L_PRESS) slotAutoMode = !slotAutoMode;
    else if (evt == EVT_R_PRESS) { slotState = SLOT_IDLE; slotMsg[0] = '\0'; }
    return false;
  }
  if (slotState == SLOT_IDLE && evt == EVT_L_PRESS) {
    slotAutoMode = !slotAutoMode;
    return false;
  }
  if (slotState == SLOT_IDLE && evt == EVT_R_PRESS) { slotSpin(); return false; }
  if (slotState == SLOT_RESULT && evt == EVT_R_PRESS) { slotSpin(); return false; }
  if (slotState == SLOT_RESULT && evt == EVT_L_PRESS) { slotState = SLOT_MODE_SELECT; return false; }
  if (slotState == SLOT_SPINNING && !slotAutoMode && evt == EVT_L_PRESS) {
    for (int i = 0; i < 3; i++) {
      if (!reelStopped[i]) {
        reelStopped[i] = true;
        reelSymbol[i] = random(SLOT_SYMBOL_COUNT);
        break;
      }
    }
    if (reelStopped[0] && reelStopped[1] && reelStopped[2]) slotSettle();
  }
  return false;
}

static void slotTick() {
  if (slotState != SLOT_SPINNING) return;
  unsigned long now = millis();
  float dt = (now - slotLastMs) / 1000.0f;
  slotLastMs = now;
  for (int i = 0; i < 3; i++) {
    if (reelStopped[i]) continue;
    reelOffset[i] += dt * 12.0f;
    if (reelOffset[i] > 1.0f) {
      reelOffset[i] -= 1.0f;
      reelSymbol[i] = random(SLOT_SYMBOL_COUNT);
    }
  }
  if (slotAutoMode) {
    unsigned long elapsed = now - slotSpinStarted;
    static const unsigned long STOP_AT[3] = {1000, 1650, 2300};
    for (int i = 0; i < 3; i++) {
      if (!reelStopped[i] && elapsed >= STOP_AT[i]) {
        reelStopped[i] = true;
        reelSymbol[i] = random(SLOT_SYMBOL_COUNT);
      }
    }
    if (reelStopped[0] && reelStopped[1] && reelStopped[2]) slotSettle();
  }
}

static void slotRender() {
  TFT_eSprite &s = uiSprite();
  drawScreenTitle("Slot Machine");

  if (slotState == SLOT_MODE_SELECT) {
    drawPanelTitled(s, 12, UI_CONTENT_Y + 48, SCREEN_W - 24, 78, "CHOOSE MODE");
    drawPixelTextC(s, slotAutoMode ? "AUTO" : "MANUAL", SCREEN_W / 2,
                   UI_CONTENT_Y + 76, 2, slotAutoMode ? Pal::GREEN_ACCENT : Pal::GOLD, Pal::INK);
    // Split into two short lines - the full sentence is wider than the
    // "CHOOSE MODE" panel (and even the screen) at size 1.
    if (slotAutoMode) {
      drawPixelTextC(s, "Reels stop", SCREEN_W / 2, UI_CONTENT_Y + 101, 1, Pal::PAPER, Pal::INK);
      drawPixelTextC(s, "themselves", SCREEN_W / 2, UI_CONTENT_Y + 112, 1, Pal::PAPER, Pal::INK);
    } else {
      drawPixelTextC(s, "Stop each reel", SCREEN_W / 2, UI_CONTENT_Y + 101, 1, Pal::PAPER, Pal::INK);
      drawPixelTextC(s, "with L", SCREEN_W / 2, UI_CONTENT_Y + 112, 1, Pal::PAPER, Pal::INK);
    }
    drawHintBar("Change", "Confirm");
    return;
  }

  int y = UI_CONTENT_Y + 60;
  int reelW = 36, gap = 6;
  int totalW = reelW * 3 + gap * 2;
  int x0 = (SCREEN_W - totalW) / 2;

  for (int i = 0; i < 3; i++) {
    int rx = x0 + i * (reelW + gap);
    drawPanel(s, rx, y, reelW, 44);
    drawSlotSymbol(s, rx + 6, y + 12, 3, reelSymbol[i]);
    if (!reelStopped[i]) s.drawRect(rx, y, reelW, 44, Pal::GOLD);
  }

  char buf[24];
  snprintf(buf, sizeof(buf), "%s  Wager:%dc", slotAutoMode ? "AUTO" : "MANUAL", SLOT_WAGER);
  drawPixelTextC(s, buf, SCREEN_W / 2, y + 56, 1, shade(Pal::PAPER, -0.2f), Pal::INK);

  if (slotState == SLOT_IDLE) {
    drawHintBar("Mode", "Spin");
  } else if (slotState == SLOT_SPINNING) {
    drawPixelTextC(s, slotAutoMode ? "Auto stopping..." : "Stop reels!", SCREEN_W / 2, y + 72, 1, Pal::PAPER, Pal::INK);
    drawHintBar(slotAutoMode ? "-" : "Stop", "-");
  } else {
    drawPixelTextC(s, slotMsg, SCREEN_W / 2, y + 76, 2, Pal::GOLD, Pal::INK);
    drawHintBar("Mode", "Spin Again");
  }
}

// ---------------------------------------------------------------------------
// Simon Says
// ---------------------------------------------------------------------------
enum SimonState { SIMON_SHOWING, SIMON_INPUT, SIMON_RESULT };
static SimonState simonState;
static int simonSeq[64];
static int simonLen;
static int simonShowIdx;
static int simonInputIdx;
static unsigned long simonTimer;
static int simonFlashPad; // -1 = none, else 0=L pad 1=R pad currently lit
static char simonMsg[24];

static void simonBegin() {
  simonLen = 1;
  simonSeq[0] = random(2);
  simonState = SIMON_SHOWING;
  simonShowIdx = 0;
  simonFlashPad = -1;
  simonTimer = millis();
}

static void simonAdvanceShow() {
  unsigned long now = millis();
  unsigned long elapsed = now - simonTimer;
  const unsigned long ON_MS = 450, GAP_MS = 200;
  if (simonFlashPad == -1) {
    if (elapsed >= GAP_MS) {
      simonFlashPad = simonSeq[simonShowIdx];
      simonTimer = now;
    }
  } else {
    if (elapsed >= ON_MS) {
      simonFlashPad = -1;
      simonShowIdx++;
      simonTimer = now;
      if (simonShowIdx >= simonLen) {
        simonState = SIMON_INPUT;
        simonInputIdx = 0;
      }
    }
  }
}

static void simonEndRound(bool success) {
  if (!success) {
    int round = simonLen - 1;
    int coins = round * 3;
    int xp = round * 4;
    addCoins(coins);
    addXP(xp);
    bumpHappiness(5);
    reportHighScore(GAME_ID_SIMON, round);
    if (round >= 10) unlockAchievement(ACH_SIMON_ROUND10);
    snprintf(simonMsg, sizeof(simonMsg), "Round %d! +%dc", round, coins);
    markSaveDirty();
    checkAchievements();
    simonState = SIMON_RESULT;
  }
}

static bool simonHandleEvent(ButtonEvent evt) {
  if (simonState == SIMON_RESULT) {
    if (evt == EVT_R_PRESS) simonBegin();
    return false;
  }
  if (simonState != SIMON_INPUT) return false;
  if (evt != EVT_L_PRESS && evt != EVT_R_PRESS) return false;

  int input = (evt == EVT_R_PRESS) ? 1 : 0;
  simonFlashPad = input;
  simonTimer = millis();

  if (input == simonSeq[simonInputIdx]) {
    simonInputIdx++;
    if (simonInputIdx >= simonLen) {
      if (simonLen >= 63) { simonEndRound(false); return false; } // safety cap
      // round clear - grow sequence and show it again
      simonSeq[simonLen] = random(2);
      simonLen++;
      simonState = SIMON_SHOWING;
      simonShowIdx = 0;
      simonFlashPad = -1;
      simonTimer = millis();
    }
  } else {
    simonEndRound(false);
  }
  return false;
}

static void simonTick() {
  if (simonState == SIMON_SHOWING) simonAdvanceShow();
  if (simonState == SIMON_INPUT && simonFlashPad != -1 && millis() - simonTimer > 150) simonFlashPad = -1;
}

static void simonRender() {
  TFT_eSprite &s = uiSprite();
  drawScreenTitle("Simon Says");

  char buf[16];
  snprintf(buf, sizeof(buf), "Round %d", simonLen);
  drawPixelTextC(s, buf, SCREEN_W / 2, UI_CONTENT_Y + 26, 1, Pal::PAPER, Pal::INK);

  int padW = 50, padH = 60, gap = 8;
  int y = UI_CONTENT_Y + 50;
  int x0 = (SCREEN_W - padW * 2 - gap) / 2;

  // Pads are labeled L/R rather than color-coded - physical button colors
  // vary by build, so the letters are the only thing that has to match.
  bool lLit = (simonFlashPad == 0);
  bool rLit = (simonFlashPad == 1);
  uint16_t padColor = Pal::PANEL_LIGHT;
  s.fillRect(x0, y, padW, padH, lLit ? shade(padColor, 0.3f) : padColor);
  s.drawRect(x0, y, padW, padH, Pal::INK);
  drawPixelTextC(s, "L", x0 + padW / 2, y + padH / 2 - 4, 2, Pal::INK, Pal::INK);
  s.fillRect(x0 + padW + gap, y, padW, padH, rLit ? shade(padColor, 0.3f) : padColor);
  s.drawRect(x0 + padW + gap, y, padW, padH, Pal::INK);
  drawPixelTextC(s, "R", x0 + padW + gap + padW / 2, y + padH / 2 - 4, 2, Pal::INK, Pal::INK);

  if (simonState == SIMON_SHOWING) {
    drawPixelTextC(s, "Watch...", SCREEN_W / 2, y + padH + 14, 1, shade(Pal::PAPER, -0.2f), Pal::INK);
    drawHintBar("-", "-");
  } else if (simonState == SIMON_INPUT) {
    drawPixelTextC(s, "Your turn!", SCREEN_W / 2, y + padH + 14, 1, Pal::PAPER, Pal::INK);
    drawHintBar("Repeat", "Repeat");
  } else {
    drawPixelTextC(s, simonMsg, SCREEN_W / 2, y + padH + 14, 1, Pal::GOLD, Pal::INK);
    drawHintBar("-", "Again");
  }
}

// ---------------------------------------------------------------------------
// dispatch
// ---------------------------------------------------------------------------
void gameBegin(GameID id) {
  activeGame = id;
  switch (id) {
    case GAME_ID_BLACKJACK: bjBegin(); break;
    case GAME_ID_HIGHLOW: hlBegin(); break;
    case GAME_ID_TIMING: tbBegin(); break;
    case GAME_ID_SLOT: slotBegin(); break;
    case GAME_ID_SIMON: simonBegin(); break;
  }
}

void gameTick() {
  if (activeGame == GAME_ID_TIMING) tbTick();
  else if (activeGame == GAME_ID_SLOT) slotTick();
  else if (activeGame == GAME_ID_SIMON) simonTick();
}

bool gameHandleEvent(ButtonEvent evt) {
  switch (activeGame) {
    case GAME_ID_BLACKJACK: return bjHandleEvent(evt);
    case GAME_ID_HIGHLOW: return hlHandleEvent(evt);
    case GAME_ID_TIMING: return tbHandleEvent(evt);
    case GAME_ID_SLOT: return slotHandleEvent(evt);
    case GAME_ID_SIMON: return simonHandleEvent(evt);
  }
  return false;
}

void gameRender() {
  switch (activeGame) {
    case GAME_ID_BLACKJACK: bjRender(); break;
    case GAME_ID_HIGHLOW: hlRender(); break;
    case GAME_ID_TIMING: tbRender(); break;
    case GAME_ID_SLOT: slotRender(); break;
    case GAME_ID_SIMON: simonRender(); break;
  }
}
