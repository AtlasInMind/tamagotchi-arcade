#include <Arduino.h>
#include "input.h"

static const unsigned long DEBOUNCE_MS = 25;
static const unsigned long HOLD_MS = 600;

struct ButtonState {
  uint8_t pin;
  int rawLast;        // last raw sample
  int stable;          // debounced state (HIGH = released, LOW = pressed)
  unsigned long lastChangeTime;
  unsigned long pressStartTime;
  bool holdFired;
};

static ButtonState lBtn = { PIN_L, HIGH, HIGH, 0, 0, false };
static ButtonState rBtn = { PIN_R, HIGH, HIGH, 0, 0, false };

void inputBegin() {
  pinMode(PIN_L, INPUT_PULLUP);
  pinMode(PIN_R, INPUT); // external pull-up on board; INPUT_PULLUP has no effect on GPIO35
}

// Updates one button's debounced state and returns a press/hold/down event,
// if any. downEvt fires immediately on the press transition (pass EVT_NONE
// for buttons that don't need it); pressEvt still fires separately on the
// matching release, unchanged.
static ButtonEvent processButton(ButtonState &b, ButtonEvent pressEvt, ButtonEvent holdEvt,
                                  ButtonEvent downEvt) {
  int raw = digitalRead(b.pin);
  unsigned long now = millis();

  if (raw != b.rawLast) {
    b.lastChangeTime = now;
    b.rawLast = raw;
  }

  if ((now - b.lastChangeTime) >= DEBOUNCE_MS && b.stable != b.rawLast) {
    b.stable = b.rawLast;
    if (b.stable == LOW) {
      // Just became pressed.
      b.pressStartTime = now;
      b.holdFired = false;
      return downEvt;
    } else {
      // Just released.
      if (!b.holdFired) {
        return pressEvt;
      }
    }
  }

  if (b.stable == LOW && !b.holdFired && (now - b.pressStartTime) >= HOLD_MS) {
    b.holdFired = true;
    return holdEvt;
  }

  return EVT_NONE;
}

ButtonEvent inputPoll() {
  ButtonEvent e = processButton(lBtn, EVT_L_PRESS, EVT_L_HOLD, EVT_NONE);
  if (e != EVT_NONE) return e;
  return processButton(rBtn, EVT_R_PRESS, EVT_R_HOLD, EVT_R_DOWN);
}
