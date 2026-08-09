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
  EVT_R_PRESS,
  EVT_R_HOLD, // reserved, not used yet
};

void inputBegin();
// Call every loop() iteration; returns at most one event per call.
ButtonEvent inputPoll();
