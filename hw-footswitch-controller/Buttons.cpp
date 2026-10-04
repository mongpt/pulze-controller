#include "Buttons.h"
#include "config.h"

Buttons buttons;

namespace {
  const uint8_t LED_PINS[4] = { PIN_LED1, PIN_LED2, PIN_LED3, PIN_LED4 };
}

void Buttons::begin() {
  // Order must match the ButtonId enum.
  const uint8_t pins[(size_t)ButtonId::COUNT] = {
    PIN_FW1, PIN_FW2, PIN_FW3, PIN_FW4,
    PIN_BANK_UP, PIN_BANK_DOWN,
    PIN_CONNECT,
    PIN_TUNER
  };
  for (size_t i = 0; i < (size_t)ButtonId::COUNT; i++) {
    _buttons[i].pin = pins[i];
    pinMode(pins[i], INPUT_PULLUP);
  }
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }
}

void Buttons::update() {
  unsigned long now = millis();

  for (size_t i = 0; i < (size_t)ButtonId::COUNT; i++) {
    ButtonState& b = _buttons[i];
    b.edgeThisUpdate = false;

    bool raw = digitalRead(b.pin); // HIGH = released, LOW = pressed (pull-up)

    if (raw != b.lastRawState) {
      b.lastChangeMs = now;
      b.lastRawState = raw;
    }

    if ((now - b.lastChangeMs) >= DEBOUNCE_MS && raw != b.stableState) {
      b.stableState = raw;
      if (raw == false) { // transitioned to pressed
        b.edgeThisUpdate = true;
      }
    }
  }
}

bool Buttons::wasPressed(ButtonId id) const {
  return _buttons[(size_t)id].edgeThisUpdate;
}

void Buttons::setLed(uint8_t fwIndex, bool on) {
  if (fwIndex >= 4) return;
  digitalWrite(LED_PINS[fwIndex], on ? HIGH : LOW);
}

void Buttons::allLedsOff() {
  for (uint8_t i = 0; i < 4; i++) digitalWrite(LED_PINS[i], LOW);
}
