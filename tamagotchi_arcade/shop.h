#pragma once
#include "input.h"

enum ShopMode {
  SHOP_MODE_BUY,     // browse all items, purchase locked ones
  SHOP_MODE_CLOSET,  // browse only owned items, equip/unequip
};

void shopBegin(ShopMode mode);
// Returns true once the shop wants to exit back to the main menu.
bool shopHandleEvent(ButtonEvent evt);
void shopRender();
