// ---------------------------------------------------------------------
// PresetStore.h - the on-flash preset library (LittleFS-backed).
//
// The library holds each distinct preset once. A PageLayout then says
// which presets sit on which page and on which footswitch, so a preset
// can appear on several pages without being stored twice.
//
// On-disk format (/presets.bin), chosen to be trivial to scan
// sequentially at boot:
//
//   [uint16 count]
//   repeated `count` times:
//     [uint16 nameLen][nameLen bytes name, UTF-8][194 bytes packet2]
//   [uint8 pageCount]
//   repeated `pageCount` times:
//     [uint8 slotCount][slotCount x uint16 presetIndex]
//
// The page table is the same byte layout the web app sends (see
// config.h). A file with no page table loads as pages of four presets
// in library order.
// ---------------------------------------------------------------------

#pragma once

#include <Arduino.h>
#include "config.h"

struct Preset {
  char name[MAX_PRESET_NAME_LEN + 1]; // +1 for null terminator
  uint8_t packet2[PACKET2_LEN];
};

// Which preset sits on which footswitch of which page. Slot n of a
// page is footswitch n+1, and a page has no gaps.
struct PageLayout {
  uint8_t pageCount;
  uint8_t slotCount[MAX_PAGES];
  uint16_t presetIndex[MAX_PAGES][PRESETS_PER_BANK]; // PRESET_NONE past slotCount

  void clear();
  void fillSequentially(uint16_t presetCount); // pages of four, in library order
  bool refersOnlyTo(uint16_t presetCount) const; // every presetIndex < presetCount
};

class PresetStore {
public:
  bool begin();                          // mounts LittleFS, loads the library into RAM
  uint16_t count() const;                // distinct presets in the library
  const Preset* get(uint16_t index) const; // nullptr if out of range

  uint16_t totalBanks() const;           // number of pages
  // Library index of the preset on this page and footswitch (offset 0
  // is footswitch 1), or PRESET_NONE when that footswitch is empty.
  uint16_t presetIndexAt(uint16_t bank, uint8_t offset) const;
  const Preset* getByBankAndOffset(uint16_t bank, uint8_t offset) const;

  // Replaces the ENTIRE library and its page layout with a freshly
  // received one and persists it to flash. Used once per transfer, not
  // per-preset - this is the only write path, keeping flash wear minimal.
  bool replaceAll(const Preset* newPresets, uint16_t newCount, const PageLayout& newLayout);

private:
  Preset _presets[MAX_LIBRARY_PRESETS];
  uint16_t _count = 0;
  PageLayout _layout;

  bool loadFromFlash();
  bool saveToFlash();
};

extern PresetStore presetStore;
