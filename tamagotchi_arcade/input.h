#pragma once

// Onboard TTGO T-Display buttons, named by position (L/R) rather than color
// since builds may use differently-colored buttons. GPIO35 is input-only
// with no internal pull-up; the board provides an external one, so it must
// stay plain INPUT.
#define PIN_L 0
#define PIN_R 35

enum ButtonEvent {
  EVT_NONE,
  EVT_L_PRESS,
  EVT_L_HOLD,
  EVT_R_PRESS,   // fires on release, like every *_PRESS event here
  EVT_R_HOLD,    // reserved, not used yet
  EVT_R_DOWN,    // fires the instant R is pressed, before release/hold/debounce
                 // resolve - for the rare case (Timing Bar) that needs a true
                 // press-down instant rather than the release-based convention
                 // every other screen relies on. The matching release still
                 // fires EVT_R_PRESS afterward; a consumer of EVT_R_DOWN must
                 // account for that trailing event itself.
};

void inputBegin();
// Call every loop() iteration; returns at most one event per call.
ButtonEvent inputPoll();
