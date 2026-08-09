#include <Arduino.h>
#include "shop.h"
#include "ui.h"
#include "gfx.h"
#include "art.h"
#include "save.h"
#include "progress.h"

static const char *const *CAT_NAMES[CAT_COUNT]  = { BACKGROUND_NAMES, HAT_NAMES, ACCESSORY_NAMES, PATTERN_NAMES };
static const int *CAT_PRICES[CAT_COUNT]         = { BACKGROUND_PRICES, HAT_PRICES, ACCESSORY_PRICES, PATTERN_PRICES };
static const int *CAT_LEVELS[CAT_COUNT]         = { BACKGROUND_LEVELS, HAT_LEVELS, ACCESSORY_LEVELS, PATTERN_LEVELS };
static const int CAT_COUNTS[CAT_COUNT]          = { BACKGROUND_COUNT, HAT_COUNT, ACCESSORY_COUNT, PATTERN_COUNT };
static const char *CAT_LABELS[CAT_COUNT]        = { "Backgrounds", "Hats", "Accessories", "Patterns" };

static ShopMode mode;
enum ShopStage { SS_CATEGORY, SS_ITEMS };
static ShopStage stage;
static int catIndex;
static int itemIndex;
static char statusMsg[24];
static unsigned long statusUntil;

// Maps visible cell -> actual item index within the category (closet mode
// hides unowned items, so the grid can be shorter than the full catalog).
static int cellToItem[32];
static int cellCount;

static void rebuildCells() {
  cellCount = 0;
  int n = CAT_COUNTS[catIndex];
  for (int i = 0; i < n; i++) {
    if (mode == SHOP_MODE_BUY || isOwned((ItemCategory)catIndex, i)) cellToItem[cellCount++] = i;
  }
  if (itemIndex >= cellCount) itemIndex = cellCount > 0 ? cellCount - 1 : 0;
}

void shopBegin(ShopMode m) {
  mode = m;
  stage = SS_CATEGORY;
  catIndex = 0;
  itemIndex = 0;
  statusMsg[0] = '\0';
}

bool shopHandleEvent(ButtonEvent evt) {
  if (stage == SS_CATEGORY) {
    if (evt == EVT_L_PRESS) catIndex = (catIndex + 1) % CAT_COUNT;
    else if (evt == EVT_R_PRESS) { itemIndex = 0; rebuildCells(); stage = SS_ITEMS; }
    else if (evt == EVT_L_HOLD) return true; // back to main menu
    return false;
  }

  // SS_ITEMS
  if (evt == EVT_L_PRESS) {
    if (cellCount > 0) itemIndex = (itemIndex + 1) % cellCount;
  } else if (evt == EVT_L_HOLD) {
    stage = SS_CATEGORY;
  } else if (evt == EVT_R_PRESS) {
    if (cellCount == 0) return false;
    int item = cellToItem[itemIndex];
    ItemCategory cat = (ItemCategory)catIndex;
    bool owned = isOwned(cat, item);

    if (!owned) {
      int reqLevel = CAT_LEVELS[catIndex][item];
      int price = CAT_PRICES[catIndex][item];
      if (game.level < reqLevel) {
        snprintf(statusMsg, sizeof(statusMsg), "Requires level %d", reqLevel);
      } else if ((uint32_t)price <= game.coins) {
        addCoins(-price);
        setOwned(cat, item);
        game.equipped[cat] = item;
        snprintf(statusMsg, sizeof(statusMsg), "Bought & equipped!");
        checkAchievements();
      } else {
        snprintf(statusMsg, sizeof(statusMsg), "Not enough coins");
      }
    } else {
      if (game.equipped[cat] == item) {
        game.equipped[cat] = 0;
        snprintf(statusMsg, sizeof(statusMsg), "Unequipped");
      } else {
        game.equipped[cat] = item;
        snprintf(statusMsg, sizeof(statusMsg), "Equipped!");
      }
    }
    markSaveDirty();
    if (mode == SHOP_MODE_CLOSET) rebuildCells();
    statusUntil = millis() + 900;
  }
  return false;
}

static void drawThumbnail(TFT_eSprite &s, ItemCategory cat, int item, int cx, int topY) {
  switch (cat) {
    case CAT_BACKGROUND:
      drawBackgroundThumbnail(s, cx - 22, topY + 1, item);
      break;
    case CAT_HAT:
      if (item == 0) {
        drawPixelTextC(s, "NO HAT", cx, topY + 12, 1, Pal::PAPER, Pal::INK);
      } else {
        drawHat(s, cx, topY + 19, 1, item);
      }
      break;
    case CAT_ACCESSORY:
      if (item == 0) {
        drawPixelTextC(s, "NONE", cx, topY + 12, 1, Pal::PAPER, Pal::INK);
      } else {
        drawAccessory(s, cx, topY + 21, cx, topY + 13, 1, item);
      }
      break;
    case CAT_PATTERN:
      drawPet(s, cx, topY + 22, 2, EXPR_NEUTRAL, game.furColor, item, 0, 0, 0);
      break;
    default: break;
  }
}

void shopRender() {
  TFT_eSprite &s = uiSprite();
  drawScreenTitle(mode == SHOP_MODE_BUY ? "Shop" : "Closet");

  if (stage == SS_CATEGORY) {
    drawMenuList(CAT_LABELS, CAT_COUNT, catIndex);
    drawHintBar("Next", "Open");
    return;
  }

  rebuildCells();
  drawPixelTextC(s, CAT_LABELS[catIndex], SCREEN_W / 2, UI_CONTENT_Y + 20, 1, shade(Pal::PAPER, -0.15f), Pal::INK);

  if (cellCount == 0) {
    drawCenteredMessage("Nothing owned yet", "Visit the Shop!");
    drawHintBar("Back", "-");
    return;
  }

  ItemCategory cat = (ItemCategory)catIndex;
  int item = cellToItem[itemIndex];

  const int cellW = 63, cellH = 48, gap = 3;
  const int gridX = 3;
  const int gridY = UI_CONTENT_Y + 32;
  const int maxVisibleRows = 3;
  int totalRows = (cellCount + 1) / 2;

  int selRow = itemIndex / 2;
  int scrollRow = 0;
  if (selRow >= maxVisibleRows) scrollRow = selRow - maxVisibleRows + 1;
  if (scrollRow > totalRows - maxVisibleRows) scrollRow = totalRows - maxVisibleRows;
  if (scrollRow < 0) scrollRow = 0;

  for (int row = 0; row < maxVisibleRows; row++) {
    for (int col = 0; col < 2; col++) {
      int i = (row + scrollRow) * 2 + col;
      if (i >= cellCount) continue;
      int cx0 = gridX + col * (cellW + gap);
      int cy0 = gridY + row * (cellH + gap);
      int it = cellToItem[i];
      bool owned = isOwned(cat, it);
      bool equipped = (game.equipped[cat] == it);
      bool selected = (i == itemIndex);
      bool locked = !owned && (game.level < CAT_LEVELS[catIndex][it]);

      if (owned && it >= 7) { // top 3 tiers get a gold trim once owned
        drawPanelAccent(s, cx0, cy0, cellW, cellH, Pal::GOLD); // premium-tier trim once owned
      } else {
        drawPanel(s, cx0, cy0, cellW, cellH);
      }
      drawThumbnail(s, cat, it, cx0 + cellW / 2, cy0 + 2);

      if (locked) {
        drawLockIcon(s, cx0 + cellW / 2 - 7, cy0 + cellH - 22, 2);
        char buf[10];
        snprintf(buf, sizeof(buf), "Lv %d", CAT_LEVELS[catIndex][it]);
        drawPixelTextC(s, buf, cx0 + cellW / 2, cy0 + cellH - 9, 1, shade(Pal::PAPER, -0.4f), Pal::INK);
      } else if (!owned) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%dc", CAT_PRICES[catIndex][it]);
        drawPixelTextC(s, buf, cx0 + cellW / 2, cy0 + cellH - 9, 1, Pal::GOLD, Pal::INK);
      } else if (equipped) {
        drawPixelTextC(s, "EQUIPPED", cx0 + cellW / 2, cy0 + cellH - 9, 1, Pal::GREEN_ACCENT, Pal::INK);
      } else {
        drawPixelTextC(s, "OWNED", cx0 + cellW / 2, cy0 + cellH - 9, 1, shade(Pal::PAPER, -0.3f), Pal::INK);
      }

      uint16_t border = selected ? Pal::RED_ACCENT : (equipped ? Pal::GREEN_ACCENT : Pal::PANEL_LIGHT);
      s.drawRect(cx0, cy0, cellW, cellH, border);
      if (selected) s.drawRect(cx0 - 1, cy0 - 1, cellW + 2, cellH + 2, border);
    }
  }

  // Scrollbar strip so position within a >3-row category is visible.
  if (totalRows > maxVisibleRows) {
    int barX = SCREEN_W - 4;
    int barH = maxVisibleRows * (cellH + gap) - gap;
    s.drawFastVLine(barX, gridY, barH, Pal::PANEL_DARK);
    int thumbH = barH * maxVisibleRows / totalRows;
    int thumbY = gridY + barH * scrollRow / totalRows;
    s.fillRect(barX - 1, thumbY, 3, thumbH, Pal::GOLD);
  }

  char nameBuf[28];
  snprintf(nameBuf, sizeof(nameBuf), "%s", CAT_NAMES[catIndex][item]);
  int bottomY = gridY + maxVisibleRows * (cellH + gap) + 1;
  if (bottomY < SCREEN_H - UI_BOTTOM_H - 9) {
    drawPixelTextC(s, nameBuf, SCREEN_W / 2, bottomY, 1, Pal::PAPER, Pal::INK);
  }

  if (statusMsg[0] && millis() < statusUntil) {
    drawPixelTextC(s, statusMsg, SCREEN_W / 2, SCREEN_H - UI_BOTTOM_H - 9, 1, Pal::GOLD, Pal::INK);
  }

  drawHintBar("Next", "Buy/Equip");
}
