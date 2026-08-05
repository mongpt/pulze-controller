// ---------------------------------------------------------------------
// Buttons.h - debounced reads for all 6 footswitches, LED control for
// the 4 preset-indicator LEDs.
// ---------------------------------------------------------------------

#pragma once

#include <Arduino.h>

enum class ButtonId {
  FW1, FW2, FW3, FW4,
  BANK_UP, BANK_DOWN,
  COUNT
};

class Buttons {
public:
  void begin();
  void update(); // call frequently (e.g. every 5-10ms) from the UI task

  // True for exactly one update() call when the button transitions
  // pressed (edge-detected, debounced) - not "is currently held".
  // Suppressed for BANK_UP/BANK_DOWN on the release that follows a
  // bankGestureTriggered() firing, so completing the hold-both gesture
  // doesn't ALSO register as a normal single-tap bank change.
  bool wasPressed(ButtonId id) const;

  // True for exactly one update() call, the moment both bank buttons
  // have been continuously held together for at least LONG_PRESS_MS -
  // used for the runtime amp-connect/disconnect gesture. Does not fire
  // again until both buttons are released and re-pressed.
  bool bankGestureTriggered() const;

  // Current (debounced) held state - used to avoid treating a press
  // that's part of forming the two-bank-button gesture as also a
  // normal single-button bank-nav tap.
  bool isHeld(ButtonId id) const;

  void setLed(uint8_t fwIndex /* 0-3 */, bool on);
  void allLedsOff();

private:
  struct ButtonState {
    uint8_t pin;
    bool stableState = true;   // true = released (pull-up, active-low)
    bool lastRawState = true;
    bool edgeThisUpdate = false;
    unsigned long lastChangeMs = 0;
  };

  ButtonState _buttons[(size_t)ButtonId::COUNT];
  unsigned long _bankHeldSinceMs = 0;
  bool _bankGestureFiredThisHold = false;
  bool _bankGestureTriggeredThisUpdate = false;
};

extern Buttons buttons;
