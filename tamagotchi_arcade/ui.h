#pragma once
#include "gfx.h"

#define UI_TOP_H 22
#define UI_BOTTOM_H 14
#define UI_CONTENT_Y (UI_TOP_H)
#define UI_CONTENT_H (SCREEN_H - UI_TOP_H - UI_BOTTOM_H)

void uiBegin();
TFT_eSprite &uiSprite();
void uiPush(); // blits the sprite to the physical display

// Persistent top status bar: level badge, XP bar, coin counter. Reads
// `game` directly (from save.h) so callers never have to pass stats through.
void drawStatusBar();

// Persistent bottom hint bar showing what L/R currently do. Badges are
// plain "L"/"R" letters with no color-coding, since physical button color
// varies by build.
void drawHintBar(const char *lHint, const char *rHint);

// Panel-framed title bar for the content area (below the status bar).
void drawScreenTitle(const char *title);

// Vertically-scrolling selectable list inside the content area.
void drawMenuList(const char **items, int count, int selectedIndex);

// Popup menu overlay used for the L-hold extended/quick menu.
void drawPopupMenu(const char **items, int count, int selectedIndex);

void drawCenteredMessage(const char *line1, const char *line2 = nullptr);
