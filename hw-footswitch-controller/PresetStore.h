// ---------------------------------------------------------------------
// PresetStore.h - the on-flash preset library (LittleFS-backed).
//
// On-disk format (/presets.bin), chosen to be trivial to append to
// during a transfer and trivial to scan sequentially at boot:
//
//   [uint16 count]
//   repeated `count` times:
//     [uint16 nameLen][nameLen bytes name, UTF-8][194 bytes packet2]
//
// This mirrors the in-memory record shape used by the web app's
// transfer protocol (BleTransfer.cpp parses directly into this same
// shape), so receiving a transfer is really just "append records,
// then rewrite the count header."
// ---------------------------------------------------------------------

#pragma once

#include <Arduino.h>
#include "config.h"

struct Preset {
  char name[MAX_PRESET_NAME_LEN + 1]; // +1 for null terminator
  uint8_t packet2[PACKET2_LEN];
};

class PresetStore {
public:
  bool begin();                          // mounts LittleFS, loads the library into RAM
  uint16_t count() const;
  const Preset* get(uint16_t index) const; // nullptr if out of range

  // Bank/footswitch math - the only place this calculation happens.
  // bank = index / PRESETS_PER_BANK, offset = index % PRESETS_PER_BANK.
  uint16_t totalBanks() const;
  const Preset* getByBankAndOffset(uint16_t bank, uint8_t offset) const;

  // Replaces the ENTIRE library with a freshly-received one and
  // persists it to flash. Used once per transfer, not per-preset -
  // this is the only write path, keeping flash wear minimal.
  bool replaceAll(const Preset* newPresets, uint16_t newCount);

private:
  static const uint16_t MAX_PRESETS = 256; // sized generously; RAM cost is small per entry
  Preset _presets[MAX_PRESETS];
  uint16_t _count = 0;

  bool loadFromFlash();
  bool saveToFlash();
};

extern PresetStore presetStore;
