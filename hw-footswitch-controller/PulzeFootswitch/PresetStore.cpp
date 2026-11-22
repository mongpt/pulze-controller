#include "PresetStore.h"
#include <LittleFS.h>

PresetStore presetStore;

bool PresetStore::begin() {
  if (!LittleFS.begin(true)) { // true = format on mount failure (first boot)
    Serial.println("[PresetStore] LittleFS mount failed");
    return false;
  }
  loadFromFlash();
  Serial.printf("[PresetStore] loaded %u preset(s)\n", _count);
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
  if (_count == 0) return 0;
  return (_count + PRESETS_PER_BANK - 1) / PRESETS_PER_BANK; // ceiling division
}

const Preset* PresetStore::getByBankAndOffset(uint16_t bank, uint8_t offset) const {
  uint32_t index = (uint32_t)bank * PRESETS_PER_BANK + offset;
  if (index >= _count) return nullptr;
  return &_presets[index];
}

bool PresetStore::replaceAll(const Preset* newPresets, uint16_t newCount) {
  if (newCount > MAX_PRESETS) {
    Serial.printf("[PresetStore] refusing %u presets, max is %u\n", newCount, MAX_PRESETS);
    return false;
  }
  memcpy(_presets, newPresets, sizeof(Preset) * newCount);
  _count = newCount;
  return saveToFlash();
}

bool PresetStore::loadFromFlash() {
  File f = LittleFS.open(PRESETS_FILE_PATH, "r");
  if (!f) {
    _count = 0;
    return false; // no file yet - fine on first boot, library just starts empty
  }

  uint16_t storedCount = 0;
  if (f.read((uint8_t*)&storedCount, 2) != 2) {
    f.close();
    _count = 0;
    return false;
  }

  uint16_t loaded = 0;
  for (uint16_t i = 0; i < storedCount && i < MAX_PRESETS; i++) {
    uint16_t nameLen = 0;
    if (f.read((uint8_t*)&nameLen, 2) != 2) break;
    if (nameLen > MAX_PRESET_NAME_LEN) nameLen = MAX_PRESET_NAME_LEN; // defensive clamp

    memset(_presets[i].name, 0, sizeof(_presets[i].name));
    if (f.read((uint8_t*)_presets[i].name, nameLen) != nameLen) break;

    if (f.read(_presets[i].packet2, PACKET2_LEN) != PACKET2_LEN) break;

    loaded++;
  }

  f.close();
  _count = loaded;
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

  f.close();
  Serial.printf("[PresetStore] saved %u preset(s) to flash\n", _count);
  return true;
}
