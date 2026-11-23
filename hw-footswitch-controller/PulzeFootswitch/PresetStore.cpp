#include "PresetStore.h"
#include <LittleFS.h>

PresetStore presetStore;

// ---- PageLayout -----------------------------------------------------------

void PageLayout::clear() {
  pageCount = 0;
  memset(slotCount, 0, sizeof(slotCount));
  memset(presetIndex, 0xFF, sizeof(presetIndex)); // 0xFFFF == PRESET_NONE
}

void PageLayout::fillSequentially(uint16_t presetCount) {
  clear();
  for (uint16_t i = 0; i < presetCount && i < MAX_PAGES * PRESETS_PER_BANK; i++) {
    uint16_t page = i / PRESETS_PER_BANK;
    presetIndex[page][slotCount[page]++] = i;
    pageCount = page + 1;
  }
}

bool PageLayout::refersOnlyTo(uint16_t presetCount) const {
  for (uint8_t page = 0; page < pageCount; page++) {
    for (uint8_t slot = 0; slot < slotCount[page]; slot++) {
      if (presetIndex[page][slot] >= presetCount) return false;
    }
  }
  return true;
}

// ---- layout file I/O ------------------------------------------------------

namespace {

// Returns false when the file has no usable page table.
bool readLayout(File& f, uint16_t presetCount, PageLayout& layout) {
  layout.clear();

  uint8_t pageCount = 0;
  if (f.read(&pageCount, 1) != 1 || pageCount > MAX_PAGES) return false;

  for (uint8_t page = 0; page < pageCount; page++) {
    uint8_t slots = 0;
    if (f.read(&slots, 1) != 1 || slots > PRESETS_PER_BANK) return false;

    for (uint8_t slot = 0; slot < slots; slot++) {
      if (f.read((uint8_t*)&layout.presetIndex[page][slot], 2) != 2) return false;
    }
    layout.slotCount[page] = slots;
  }
  layout.pageCount = pageCount;

  return layout.refersOnlyTo(presetCount);
}

void writeLayout(File& f, const PageLayout& layout) {
  f.write(&layout.pageCount, 1);
  for (uint8_t page = 0; page < layout.pageCount; page++) {
    f.write(&layout.slotCount[page], 1);
    for (uint8_t slot = 0; slot < layout.slotCount[page]; slot++) {
      f.write((const uint8_t*)&layout.presetIndex[page][slot], 2);
    }
  }
}

} // namespace

// ---- PresetStore ----------------------------------------------------------

bool PresetStore::begin() {
  if (!LittleFS.begin(true)) { // true = format on mount failure (first boot)
    Serial.println("[PresetStore] LittleFS mount failed");
    return false;
  }
  loadFromFlash();
  Serial.printf("[PresetStore] loaded %u preset(s) on %u page(s)\n", _count, _layout.pageCount);
  return true;
}

uint16_t PresetStore::count() const {
  return _count;
}

const Preset* PresetStore::get(uint16_t index) const {
  if (index >= _count) return nullptr;
  return &_presets[index];
}

uint16_t PresetStore::totalBanks() const {
  return _layout.pageCount;
}

uint16_t PresetStore::presetIndexAt(uint16_t bank, uint8_t offset) const {
  if (bank >= _layout.pageCount) return PRESET_NONE;
  if (offset >= _layout.slotCount[bank]) return PRESET_NONE;
  return _layout.presetIndex[bank][offset];
}

const Preset* PresetStore::getByBankAndOffset(uint16_t bank, uint8_t offset) const {
  return get(presetIndexAt(bank, offset)); // get() returns nullptr for PRESET_NONE
}

bool PresetStore::replaceAll(const Preset* newPresets, uint16_t newCount, const PageLayout& newLayout) {
  if (newCount > MAX_LIBRARY_PRESETS) {
    Serial.printf("[PresetStore] refusing %u presets, max is %u\n", newCount, MAX_LIBRARY_PRESETS);
    return false;
  }
  if (!newLayout.refersOnlyTo(newCount)) {
    Serial.println("[PresetStore] refusing page layout that points past the library");
    return false;
  }
  memcpy(_presets, newPresets, sizeof(Preset) * newCount);
  _count = newCount;
  _layout = newLayout;
  return saveToFlash();
}

bool PresetStore::loadFromFlash() {
  _count = 0;
  _layout.clear();

  File f = LittleFS.open(PRESETS_FILE_PATH, "r");
  if (!f) {
    return false; // no file yet - fine on first boot, library just starts empty
  }

  uint16_t storedCount = 0;
  if (f.read((uint8_t*)&storedCount, 2) != 2) {
    f.close();
    return false;
  }

  uint16_t loaded = 0;
  for (uint16_t i = 0; i < storedCount && i < MAX_LIBRARY_PRESETS; i++) {
    uint16_t nameLen = 0;
    if (f.read((uint8_t*)&nameLen, 2) != 2) break;
    if (nameLen > MAX_PRESET_NAME_LEN) nameLen = MAX_PRESET_NAME_LEN; // defensive clamp

    memset(_presets[i].name, 0, sizeof(_presets[i].name));
    if (f.read((uint8_t*)_presets[i].name, nameLen) != nameLen) break;

    if (f.read(_presets[i].packet2, PACKET2_LEN) != PACKET2_LEN) break;

    loaded++;
  }
  _count = loaded;

  // Only read the page table after a clean read of every record.
  // Files saved before pages existed have no table and use pages of four.
  bool recordsComplete = (loaded == storedCount);
  if (!recordsComplete || !readLayout(f, _count, _layout)) {
    _layout.fillSequentially(_count);
  }

  f.close();
  return true;
}

bool PresetStore::saveToFlash() {
  File f = LittleFS.open(PRESETS_FILE_PATH, "w");
  if (!f) {
    Serial.println("[PresetStore] failed to open presets file for writing");
    return false;
  }

  f.write((const uint8_t*)&_count, 2);
  for (uint16_t i = 0; i < _count; i++) {
    uint16_t nameLen = strnlen(_presets[i].name, MAX_PRESET_NAME_LEN);
    f.write((const uint8_t*)&nameLen, 2);
    f.write((const uint8_t*)_presets[i].name, nameLen);
    f.write(_presets[i].packet2, PACKET2_LEN);
  }
  writeLayout(f, _layout);

  f.close();
  Serial.printf("[PresetStore] saved %u preset(s) on %u page(s) to flash\n", _count, _layout.pageCount);
  return true;
}
