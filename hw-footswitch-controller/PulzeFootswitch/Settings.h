// ---------------------------------------------------------------------
// Settings.h - small, frequently-touched values via Preferences (NVS).
// Deliberately NOT LittleFS - this is the wear-leveled mechanism meant
// for exactly this kind of small-value, occasionally-written data.
// ---------------------------------------------------------------------

#pragma once

#include <Arduino.h>

class Settings {
public:
  void begin();

  // Empty string if never connected before.
  String getAmpAddress() const;
  uint8_t getAmpAddressType() const; // BLE_ADDR_PUBLIC / BLE_ADDR_RANDOM
  void setAmpAddress(const String& address, uint8_t addressType);

  // The footswitch last used, as page * PRESETS_PER_BANK + footswitch
  // (0-based). 0xFFFF = none.
  uint16_t getLastActiveSlot() const;
  void setLastActiveSlot(uint16_t slot);
};

extern Settings settings;
