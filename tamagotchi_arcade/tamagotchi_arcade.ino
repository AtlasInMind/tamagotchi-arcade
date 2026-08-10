// Tamagotchi Arcade v3 — ESP32 TTGO T-Display (portrait, pixel art)
// Two-button pixel-art pet game: create your companion, play minigames to
// earn coins/XP, spend coins on cosmetics for the pet.
// L (GPIO0)  = Next (press) / Options menu (press, Home only)
// R (GPIO35) = Select / Confirm (press)

#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include "input.h"
#include "gfx.h"
#include "ui.h"
#include "save.h"
#include "art.h"
#include "games.h"
#include "shop.h"
#include "progress.h"

// NOTE: all types used in any function signature below are defined here,
// immediately after the includes. The Arduino builder auto-generates
// forward prototypes for every function and inserts them right after the
// include block - if a struct/enum parameter type were defined further
// down the file, that generated prototype would reference an as-yet-unknown
// type and fail to compile.
enum AppState {
  ST_CREATE_COLOR,
  ST_SPLASH,
  ST_HOME,
  ST_MENU,
  ST_ARCADE_LIST,
  ST_GAME,
  ST_SHOP,
  ST_CLOSET,
  ST_RECORDS,
  ST_CONFIRM_RESET,
};

enum PopupAction { ACT_CANCEL, ACT_QUIT_GAME, ACT_CASHOUT, ACT_RECORDS, ACT_RESET };
struct PopupItem { const char *label; PopupAction action; };

static AppState state;
static GameID currentGameId;

static void changeState(AppState s) {
  state = s;
}

// ---- popup (Home's quick menu and in-game Quit/Cash Out) ----
static bool popupOpen = false;
static int popupSel = 0;
static PopupItem popupItems[4];
static int popupCount = 0;

static void openPopup(const PopupItem *items, int count) {
  for (int i = 0; i < count; i++) popupItems[i] = items[i];
  popupCount = count;
  popupSel = 0;
  popupOpen = true;
}

static const char **popupLabels() {
  static const char *labels[4];
  for (int i = 0; i < popupCount; i++) labels[i] = popupItems[i].label;
  return labels;
}

// ---- menu list definitions ----
static const char *MAIN_MENU_ITEMS[] = { "Arcade", "Shop", "Closet", "Records", "Back to Pet" };
static const int MAIN_MENU_COUNT = 5;
static int mainMenuSel = 0;
static int recordsPage = 0;
static const int ACHIEVEMENTS_PER_PAGE = 7;

static const char *ARCADE_ITEMS[] = { "Blackjack", "High-Low", "Timing Bar", "Slot Machine", "Simon Says", "Back" };
static const int ARCADE_COUNT = 6;
static int arcadeSel = 0;

// ---- character creation state ----
static int createColorSel = 0;

// ---- Home screen idle animation ----
static unsigned long lastBlinkAt = 0;
static bool blinking = false;
static unsigned long lastSecondTick = 0;
static unsigned long lastFrameMs = 0;
static float gDt = 0.016f; // seconds since last frame, for ambient background effects
static unsigned long lastInteractionMs = 0;
static const unsigned long SLEEP_AFTER_MS = 120000;
static bool wasAsleep = false;

static bool isAsleepNow() {
  return millis() - lastInteractionMs >= SLEEP_AFTER_MS;
}

// ---------------------------------------------------------------------------
// Offline/elapsed-time model.
//
// This board has no battery-backed RTC chip and no WiFi/NTP, so there is no
// wall-clock source that survives a real power loss (unplug, dead battery) -
// on a true power-on reset these RTC_DATA_ATTR values come back zeroed and
// we correctly fall back to "no catch-up, just resume". What we *do* have is
// the RTC domain that stays powered across deep sleep, so instead of only
// dimming the screen after a long idle we deep-sleep the chip: the sleep
// interval is then real elapsed time we can measure with esp_timer_get_time()
// (its base carries across deep sleep) and use to dock happiness for the time
// the pet was left alone, the way the neglect mechanic works on real
// Tamagotchi hardware. A soft reset (crash/watchdog/EN button) is not a deep
// sleep wake, so it's treated the same as a power loss: no catch-up.
//
// Only the R button (GPIO35) is wired as the wake source. GPIO0 (L) is this
// board's flash/boot-strap pin - holding it low at reset time (which is what
// waking would require) risks dropping the board into UART download mode
// instead of booting the app, so it's deliberately never used for wakeup.
static const unsigned long DEEP_SLEEP_AFTER_MS = 300000; // 5 minutes idle
RTC_DATA_ATTR static int64_t rtcSleepStartUs = 0;
RTC_DATA_ATTR static bool rtcHasSleepSnapshot = false;

// Applies happiness decay for time spent deep-asleep, at the same rate as
// the online decay in tickPlaytimeAndDecay() (1 point/minute). Sets
// *wokeFromSleep so the caller can tell a real resume from a fresh/power-loss
// boot, and returns the elapsed offline seconds (0 when *wokeFromSleep is
// false, or when the sleep was too short to matter).
static unsigned long applyOfflineCatchUp(bool *wokeFromSleep) {
  *wokeFromSleep = rtcHasSleepSnapshot &&
      esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
  rtcHasSleepSnapshot = false; // consume the snapshot so a later soft reset can't reapply it
  if (!*wokeFromSleep) return 0;

  int64_t elapsedUs = esp_timer_get_time() - rtcSleepStartUs;
  if (elapsedUs < 0) return 0; // defensive; esp_timer's base should only move forward
  unsigned long elapsedSec = (unsigned long)(elapsedUs / 1000000);
  int decayPoints = (int)(elapsedSec / 60);
  if (decayPoints > 0) bumpHappiness(-decayPoints);
  return elapsedSec;
}

static void enterDeepSleepIfIdle() {
  if (millis() - lastInteractionMs < DEEP_SLEEP_AFTER_MS) return;
  saveFlushNow();
  rtcSleepStartUs = esp_timer_get_time();
  rtcHasSleepSnapshot = true;
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_R, 0); // wake on R going LOW (pressed)
  esp_deep_sleep_start(); // never returns; next boot re-enters setup()
}

static PetExpression moodExpression() {
  if (isAsleepNow()) return EXPR_ASLEEP;
  if (blinking) return EXPR_BLINK;
  if (game.happiness >= 70) return EXPR_HAPPY;
  if (game.happiness < 35) return EXPR_SAD;
  return EXPR_NEUTRAL;
}

static void renderHome() {
  TFT_eSprite &s = uiSprite();
  drawBackground(s, game.equipped[CAT_BACKGROUND], gDt);
  drawStatusBar();

  drawPet(s, SCREEN_W / 2, 140, 5, moodExpression(),
          game.furColor, game.equipped[CAT_PATTERN],
          game.equipped[CAT_HAT], game.equipped[CAT_ACCESSORY], 0);

  drawHintBar("Options", "Menu");
}

static void tickHomeAnimation() {
  unsigned long now = millis();
  if (blinking && now - lastBlinkAt > 150) { blinking = false; lastBlinkAt = now; }
  else if (!blinking && now - lastBlinkAt > 3000) { blinking = true; lastBlinkAt = now; }
}

static void tickPlaytimeAndDecay() {
  unsigned long now = millis();
  if (now - lastSecondTick >= 1000) {
    lastSecondTick = now;
    game.totalPlaySeconds++;
    if (game.totalPlaySeconds % 60 == 0 && game.happiness > 0) game.happiness--;
    if (game.totalPlaySeconds % 5 == 0) checkAchievements();
  }
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Tamagotchi Arcade v3 booting...");

  randomSeed(esp_random());
  inputBegin();
  uiBegin();
  saveBegin();
  particlesReset();
  lastInteractionMs = millis();

  bool wokeFromSleep = false;
  unsigned long offlineSec = game.created ? applyOfflineCatchUp(&wokeFromSleep) : 0;
  if (wokeFromSleep) {
    if (offlineSec >= 60) {
      markSaveDirty();
      char line2[32];
      snprintf(line2, sizeof(line2), "Away %lum - happiness dipped", offlineSec / 60);
      toastShow("Welcome back", line2, Pal::GOLD);
    }
    state = ST_HOME; // skip the splash screen on a wake, it's not a fresh boot
  } else {
    state = game.created ? ST_SPLASH : ST_CREATE_COLOR;
  }

  Serial.printf("Loaded save: coins=%u xp=%u level=%u happiness=%u furColor=%d created=%d achievements=%d/%d\n",
                game.coins, game.xp, game.level, game.happiness, game.furColor, game.created,
                achievementCountUnlocked(), (int)ACH_COUNT);
}

// ---------------------------------------------------------------------------
static void handlePopupEvent(ButtonEvent evt) {
  if (evt == EVT_L_PRESS) {
    popupSel = (popupSel + 1) % popupCount;
  } else if (evt == EVT_L_HOLD) {
    popupOpen = false;
  } else if (evt == EVT_R_PRESS) {
    PopupAction action = popupItems[popupSel].action;
    popupOpen = false;
    switch (action) {
      case ACT_CANCEL: break;
      case ACT_QUIT_GAME: saveFlushNow(); changeState(ST_ARCADE_LIST); break;
      case ACT_CASHOUT: highLowCashOut(); break;
      case ACT_RECORDS: recordsPage = 0; changeState(ST_RECORDS); break;
      case ACT_RESET: changeState(ST_CONFIRM_RESET); break;
    }
  }
}

static void finishCreation() {
  markCreated((uint8_t)createColorSel);
  changeState(ST_HOME);
}

static void handleEvent(ButtonEvent evt) {
  if (evt == EVT_NONE) return;
  lastInteractionMs = millis(); // any physical interaction wakes the cat

  if (popupOpen) { handlePopupEvent(evt); return; }

  switch (state) {
    case ST_CREATE_COLOR:
      if (evt == EVT_L_PRESS) createColorSel = (createColorSel + 1) % FUR_COLOR_COUNT;
      else if (evt == EVT_R_PRESS) finishCreation();
      break;

    case ST_SPLASH:
      if (evt == EVT_R_PRESS || evt == EVT_L_PRESS) changeState(ST_HOME);
      break;

    case ST_HOME:
      if (evt == EVT_R_PRESS) { mainMenuSel = 0; changeState(ST_MENU); }
      else if (evt == EVT_L_PRESS) {
        static const PopupItem items[] = {
          { "Records", ACT_RECORDS }, { "Reset Save", ACT_RESET }, { "Cancel", ACT_CANCEL }
        };
        openPopup(items, 3);
      }
      break;

    case ST_MENU:
      if (evt == EVT_L_PRESS) mainMenuSel = (mainMenuSel + 1) % MAIN_MENU_COUNT;
      else if (evt == EVT_L_HOLD) changeState(ST_HOME);
      else if (evt == EVT_R_PRESS) {
        switch (mainMenuSel) {
          case 0: arcadeSel = 0; changeState(ST_ARCADE_LIST); break;
          case 1: shopBegin(SHOP_MODE_BUY); changeState(ST_SHOP); break;
          case 2: shopBegin(SHOP_MODE_CLOSET); changeState(ST_CLOSET); break;
          case 3: recordsPage = 0; changeState(ST_RECORDS); break;
          case 4: changeState(ST_HOME); break;
        }
      }
      break;

    case ST_ARCADE_LIST:
      if (evt == EVT_L_PRESS) arcadeSel = (arcadeSel + 1) % ARCADE_COUNT;
      else if (evt == EVT_L_HOLD) changeState(ST_MENU);
      else if (evt == EVT_R_PRESS) {
        if (arcadeSel == ARCADE_COUNT - 1) {
          changeState(ST_MENU);
        } else {
          currentGameId = (GameID)arcadeSel; // enum order matches list order
          gameBegin(currentGameId);
          changeState(ST_GAME);
        }
      }
      break;

    case ST_GAME:
      if (evt == EVT_L_HOLD) {
        if (currentGameId == GAME_ID_HIGHLOW) {
          static const PopupItem items[] = {
            { "Cash Out", ACT_CASHOUT }, { "Quit", ACT_QUIT_GAME }, { "Cancel", ACT_CANCEL }
          };
          openPopup(items, 3);
        } else {
          static const PopupItem items[] = { { "Quit", ACT_QUIT_GAME }, { "Cancel", ACT_CANCEL } };
          openPopup(items, 2);
        }
      } else {
        gameHandleEvent(evt);
      }
      break;

    case ST_SHOP:
      if (shopHandleEvent(evt)) changeState(ST_MENU);
      break;

    case ST_CLOSET:
      if (shopHandleEvent(evt)) changeState(ST_MENU);
      break;

    case ST_RECORDS:
      if (evt == EVT_L_PRESS) {
        int pageCount = 1 + (ACH_COUNT + ACHIEVEMENTS_PER_PAGE - 1) / ACHIEVEMENTS_PER_PAGE;
        recordsPage = (recordsPage + 1) % pageCount;
      } else if (evt == EVT_L_HOLD || evt == EVT_R_PRESS) {
        changeState(ST_MENU);
      }
      break;

    case ST_CONFIRM_RESET:
      if (evt == EVT_R_PRESS) { saveResetProgress(); state = game.created ? ST_SPLASH : ST_CREATE_COLOR; }
      else if (evt == EVT_L_PRESS || evt == EVT_L_HOLD) changeState(ST_HOME);
      break;
  }
}

// ---------------------------------------------------------------------------
static void renderCreateColor() {
  TFT_eSprite &s = uiSprite();
  drawBackground(s, 2, gDt); // meadow - neutral, welcoming
  drawPixelTextC(s, "Pick your cat", SCREEN_W / 2, 14, 1, Pal::PAPER, Pal::INK);

  drawPet(s, SCREEN_W / 2, 132, 5, EXPR_HAPPY, createColorSel, 0, 0, 0, 0);

  drawPixelTextC(s, FUR_COLOR_NAMES[createColorSel], SCREEN_W / 2, 198, 2, Pal::GOLD, Pal::INK);
  drawHintBar("Next", "Confirm");
}

static void renderSplash() {
  TFT_eSprite &s = uiSprite();
  drawBackground(s, 1, gDt); // starry night
  drawPet(s, SCREEN_W / 2, 88, 5, EXPR_HAPPY, game.furColor,
          game.equipped[CAT_PATTERN], 0, 0, 0);
  drawPixelTextC(s, "TAMAGOTCHI", SCREEN_W / 2, SCREEN_H / 2 + 55, 2, Pal::PAPER, Pal::INK);
  drawPixelTextC(s, "ARCADE", SCREEN_W / 2, SCREEN_H / 2 + 75, 2, Pal::GOLD, Pal::INK);
  if ((millis() / 400) % 2 == 0) {
    drawPixelTextC(s, "PRESS A BUTTON", SCREEN_W / 2, SCREEN_H - 18, 1, shade(Pal::PAPER, -0.2f), Pal::INK);
  }
}

static void renderRecords() {
  TFT_eSprite &s = uiSprite();
  int achievementPages = (ACH_COUNT + ACHIEVEMENTS_PER_PAGE - 1) / ACHIEVEMENTS_PER_PAGE;
  int pageCount = 1 + achievementPages;
  drawScreenTitle(recordsPage == 0 ? "Records" : "Achievements");
  s.setTextDatum(TL_DATUM);
  int y = UI_CONTENT_Y + 26;
  char buf[40];

  if (recordsPage == 0) {
    snprintf(buf, sizeof(buf), "Lv %d  XP %u/%d", game.level, game.xp, xpForNextLevel(game.level));
    drawPixelText(s, buf, 8, y, 1, Pal::PAPER, Pal::INK); y += 13;
    snprintf(buf, sizeof(buf), "Happiness: %d%%", game.happiness);
    drawPixelText(s, buf, 8, y, 1, Pal::PAPER, Pal::INK); y += 13;
    snprintf(buf, sizeof(buf), "Playtime: %lum", game.totalPlaySeconds / 60);
    drawPixelText(s, buf, 8, y, 1, Pal::PAPER, Pal::INK); y += 18;
    drawPixelText(s, "HIGH SCORES", 8, y, 1, Pal::GOLD, Pal::INK); y += 14;
    for (int i = 0; i < NUM_MINIGAMES; i++) {
      snprintf(buf, sizeof(buf), "%s: %u", GAME_NAMES[i], (unsigned)game.highScores[i]);
      drawPixelText(s, buf, 8, y, 1, shade(Pal::PAPER, -0.15f), Pal::INK);
      y += 13;
    }
    y += 5;
    snprintf(buf, sizeof(buf), "Unlocked: %d/%d", achievementCountUnlocked(), (int)ACH_COUNT);
    drawPixelText(s, buf, 8, y, 1, Pal::GOLD, Pal::INK);
  } else {
    int start = (recordsPage - 1) * ACHIEVEMENTS_PER_PAGE;
    int end = min(start + ACHIEVEMENTS_PER_PAGE, (int)ACH_COUNT);
    for (int i = start; i < end; i++) {
      bool got = hasAchievement((AchievementId)i);
      drawPanel(s, 5, y - 2, SCREEN_W - 10, 20);
      snprintf(buf, sizeof(buf), "%s %s", got ? "[X]" : "[ ]", ACHIEVEMENTS[i].name);
      drawPixelText(s, buf, 10, y + 3, 1,
                    got ? Pal::GREEN_ACCENT : shade(Pal::PAPER, -0.45f), Pal::INK);
      y += 22;
    }
  }

  snprintf(buf, sizeof(buf), "%d/%d", recordsPage + 1, pageCount);
  drawPixelTextC(s, buf, SCREEN_W / 2, SCREEN_H - UI_BOTTOM_H - 11, 1, Pal::GOLD, Pal::INK);
  drawHintBar("Next Page", "Back");
}

static void renderConfirmReset() {
  drawScreenTitle("Reset Save");
  drawCenteredMessage("Erase all progress?", "This cannot be undone");
  drawHintBar("No", "Yes, erase");
}

static void render() {
  TFT_eSprite &s = uiSprite();
  switch (state) {
    case ST_CREATE_COLOR:
      renderCreateColor();
      break;
    case ST_SPLASH:
      renderSplash();
      break;
    case ST_HOME:
      renderHome();
      break;
    case ST_MENU:
      drawBackground(s, game.equipped[CAT_BACKGROUND], gDt);
      drawStatusBar();
      drawScreenTitle("Main Menu");
      drawMenuList(MAIN_MENU_ITEMS, MAIN_MENU_COUNT, mainMenuSel);
      drawHintBar("Next", "Select");
      break;
    case ST_ARCADE_LIST:
      drawBackground(s, game.equipped[CAT_BACKGROUND], gDt);
      drawStatusBar();
      drawScreenTitle("Arcade");
      drawMenuList(ARCADE_ITEMS, ARCADE_COUNT, arcadeSel);
      drawHintBar("Next", "Play");
      break;
    case ST_GAME:
      drawBackground(s, game.equipped[CAT_BACKGROUND], gDt);
      drawStatusBar();
      gameRender(); // draws its own hint bar per-state
      break;
    case ST_SHOP:
    case ST_CLOSET:
      drawBackground(s, game.equipped[CAT_BACKGROUND], gDt);
      drawStatusBar();
      shopRender(); // draws its own hint bar per-stage
      break;
    case ST_RECORDS:
      drawBackground(s, game.equipped[CAT_BACKGROUND], gDt);
      drawStatusBar();
      renderRecords();
      break;
    case ST_CONFIRM_RESET:
      drawBackground(s, game.equipped[CAT_BACKGROUND], gDt);
      drawStatusBar();
      renderConfirmReset();
      break;
  }

  particlesRender(s);
  if (popupOpen) drawPopupMenu(popupLabels(), popupCount, popupSel);
  toastRender(s);
}

void loop() {
  unsigned long now = millis();
  gDt = (lastFrameMs == 0) ? 0.016f : (now - lastFrameMs) / 1000.0f;
  lastFrameMs = now;
  if (gDt > 0.1f) gDt = 0.1f; // clamp huge gaps (e.g. after upload reset)

  ButtonEvent evt = inputPoll();
  handleEvent(evt);

  tickPlaytimeAndDecay();

  // Flush once on the awake->asleep transition - a natural "user walked
  // away" checkpoint - in addition to saveTick()'s regular debounce timer.
  bool asleepNow = isAsleepNow();
  if (asleepNow && !wasAsleep) saveFlushNow();
  wasAsleep = asleepNow;
  saveTick();

  enterDeepSleepIfIdle(); // no-op until far past asleepNow's own threshold

  if (state == ST_HOME) tickHomeAnimation();
  if (state == ST_GAME) gameTick();
  particlesTick(gDt);
  toastTick(gDt);

  render();
  uiPush();
}
