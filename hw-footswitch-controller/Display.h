// ---------------------------------------------------------------------
// Display.h - 128x64 I2C SSD1306, via the widely-used Adafruit_SSD1306 +
// Adafruit_GFX libraries (install both via Library Manager).
// ---------------------------------------------------------------------

#pragma once

#include <Arduino.h>
#include "BleAmp.h"

class Display {
public:
  bool begin();

  void showBoot();
  void showPerformance(AmpConnState ampState, uint16_t bank, uint8_t offset,
                        uint16_t totalBanks, const char* presetName);
  void showTransferWaiting();
  void showReceiving();
  void showTransferProgress(uint16_t received, uint16_t total);
  void showTransferDone(uint16_t count);
  void showError(const char* message);
};

extern Display display;
