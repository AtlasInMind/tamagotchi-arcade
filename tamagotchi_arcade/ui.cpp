#include "ui.h"
#include "save.h"
#include "art.h"

static TFT_eSPI tft = TFT_eSPI();
static TFT_eSprite spr = TFT_eSprite(&tft);

void uiBegin() {
  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.fillScreen(Pal::BG);
  spr.setColorDepth(16);
  spr.createSprite(SCREEN_W, SCREEN_H);
  spr.setTextDatum(TL_DATUM);
}

TFT_eSprite &uiSprite() { return spr; }
void uiPush() { spr.pushSprite(0, 0); }

void drawStatusBar() {
  spr.fillRect(0, 0, SCREEN_W, UI_TOP_H, Pal::PANEL);
  spr.drawFastHLine(0, UI_TOP_H - 1, SCREEN_W, Pal::PANEL_DARK);
  spr.drawFastHLine(0, UI_TOP_H, SCREEN_W, Pal::PANEL_LIGHT);

  // Level badge: a small gold medallion instead of plain text.
  int badgeCx = 11, badgeCy = 11, badgeR = 8;
  spr.fillCircle(badgeCx, badgeCy, badgeR, Pal::GOLD);
  spr.drawCircle(badgeCx, badgeCy, badgeR, shade(Pal::GOLD, -0.5f));
  spr.fillCircle(badgeCx - 2, badgeCy - 2, 2, shade(Pal::GOLD, 0.5f));
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", game.level);
  drawPixelTextC(spr, buf, badgeCx, badgeCy - 3, 1, Pal::INK, Pal::INK);

  // XP bar
  int barX = 24, barY = 9, barW = 68, barH = 5;
  int need = xpForNextLevel(game.level);
  float pct = need > 0 ? (float)game.xp / need : 0;
  if (pct > 1) pct = 1;
  spr.fillRect(barX, barY, barW, barH, shade(Pal::BG, -0.2f));
  spr.fillRect(barX, barY, (int)(barW * pct), barH, Pal::GREEN_ACCENT);
  spr.drawRect(barX - 1, barY - 1, barW + 2, barH + 2, Pal::PANEL_DARK);

  // Coins
  snprintf(buf, sizeof(buf), "%u", (unsigned)game.coins);
  int textW = pixelTextWidth(buf, 1);
  int coinX = SCREEN_W - 6 - textW - 8;
  drawCoinIcon(spr, coinX, 10, 1);
  drawPixelText(spr, buf, coinX + 6, 7, 1, Pal::GOLD, Pal::INK);
}

// Badges read "L"/"R" only - deliberately no color-coding, since builds may
// use differently-colored physical buttons for these two roles.
static void drawButtonBadge(int x, int y, const char *letter) {
  spr.fillRect(x, y, 9, 9, Pal::PANEL_DARK);
  spr.drawRect(x, y, 9, 9, Pal::PANEL_LIGHT);
  drawPixelText(spr, letter, x + 2, y + 1, 1, Pal::PAPER, Pal::PAPER);
}

void drawHintBar(const char *lHint, const char *rHint) {
  int y = SCREEN_H - UI_BOTTOM_H;
  spr.fillRect(0, y, SCREEN_W, UI_BOTTOM_H, Pal::PANEL);
  spr.drawFastHLine(0, y, SCREEN_W, Pal::PANEL_LIGHT);
  int by = y + (UI_BOTTOM_H - 9) / 2;

  drawButtonBadge(3, by, "L");
  drawPixelText(spr, lHint, 14, y + 3, 1, Pal::PAPER, Pal::PAPER);

  int gw = pixelTextWidth(rHint, 1);
  int gx = SCREEN_W - 14 - gw;
  drawButtonBadge(gx - 12, by, "R");
  drawPixelText(spr, rHint, gx, y + 3, 1, Pal::PAPER, Pal::PAPER);
}

void drawScreenTitle(const char *title) {
  drawPanelTitled(spr, 2, UI_CONTENT_Y + 2, SCREEN_W - 4, 18, title);
}

void drawMenuList(const char **items, int count, int selectedIndex) {
  const int x = 4, y = UI_CONTENT_Y + 4;
  const int w = SCREEN_W - 8;
  const int rowH = 20;
  const int maxVisible = (UI_CONTENT_H - 8) / rowH;

  int scrollOffset = 0;
  if (selectedIndex >= maxVisible) scrollOffset = selectedIndex - maxVisible + 1;
  if (scrollOffset > count - maxVisible) scrollOffset = count - maxVisible;
  if (scrollOffset < 0) scrollOffset = 0;

  int visibleCount = min(maxVisible, count);
  drawPanel(spr, x, y, w, visibleCount * rowH + 4);

  for (int row = 0; row < maxVisible; row++) {
    int i = row + scrollOffset;
    if (i >= count) break;
    int ry = y + 2 + row * rowH;
    bool sel = (i == selectedIndex);
    if (sel) {
      spr.fillRect(x + 2, ry, w - 4, rowH, shade(Pal::PANEL, 0.35f));
      spr.fillRect(x + 2, ry, 3, rowH, Pal::GREEN_ACCENT);
    }
    drawPixelText(spr, items[i], x + 10, ry + 6, 1, sel ? Pal::PAPER : shade(Pal::PAPER, -0.25f), Pal::INK);
  }
}

void drawPopupMenu(const char **items, int count, int selectedIndex) {
  int rowH = 18;
  int boxW = 96, boxH = 16 + count * rowH + 6;
  int x = (SCREEN_W - boxW) / 2;
  int y = (SCREEN_H - boxH) / 2;

  drawPanelTitled(spr, x, y, boxW, boxH, "MENU");
  for (int i = 0; i < count; i++) {
    int ry = y + 18 + i * rowH;
    bool sel = (i == selectedIndex);
    if (sel) {
      spr.fillRect(x + 3, ry, boxW - 6, rowH - 2, shade(Pal::PANEL, 0.35f));
      spr.fillRect(x + 3, ry, 3, rowH - 2, Pal::RED_ACCENT);
    }
    drawPixelText(spr, items[i], x + 10, ry + 5, 1, sel ? Pal::PAPER : shade(Pal::PAPER, -0.25f), Pal::INK);
  }
}

void drawCenteredMessage(const char *line1, const char *line2) {
  int y = SCREEN_H / 2 - (line2 ? 12 : 4);
  drawPixelTextC(spr, line1, SCREEN_W / 2, y, 2, Pal::PAPER, Pal::INK);
  if (line2) drawPixelTextC(spr, line2, SCREEN_W / 2, y + 20, 1, shade(Pal::PAPER, -0.2f), Pal::INK);
}
