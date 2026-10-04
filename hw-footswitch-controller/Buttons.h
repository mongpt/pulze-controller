// ---------------------------------------------------------------------
// Buttons.h - debounced reads for all 8 footswitches (4 presets, Bank
// Up/Down, Connect, Tuner), LED control for the 4 preset-indicator LEDs.
//
// Every button is a plain single-tap input - there are no multi-button
// gestures, so each switch does exactly one thing.
// ---------------------------------------------------------------------

#pragma once

#include <Arduino.h>

enum class ButtonId {
  FW1, FW2, FW3, FW4,
  BANK_UP, BANK_DOWN,
  CONNECT,
  TUNER,
  COUNT
};

class Buttons {
public:
  void begin();
  void update(); // call frequently (e.g. every 5-10ms) from the UI task

  // True for exactly one update() call when the button transitions
  // pressed (edge-detected, debounced) - not "is currently held".
  bool wasPressed(ButtonId id) const;

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
};

extern Buttons buttons;
