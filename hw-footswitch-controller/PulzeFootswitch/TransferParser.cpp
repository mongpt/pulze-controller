// ---------------------------------------------------------------------
// TransferParser.cpp - pure logic, no BLE/hardware dependencies.
//
// This file intentionally does not touch NimBLE, LittleFS, or Serial -
// it only reads bytes out of a buffer and writes Preset structs and a
// PageLayout. The byte layout it reads is documented in config.h.
// ---------------------------------------------------------------------

#include "BleTransfer.h"
#include <cstring>

namespace {

// Reads the page table at the start of the buffer. Returns false when
// it is truncated or malformed. On success `offset` is just past it.
bool parsePageTable(
  const uint8_t* buffer, size_t bufferLen,
  uint16_t presetCount,
  PageLayout& layout, size_t& offset
) {
  layout.clear();

  if (offset + 1 > bufferLen) return false;
  uint8_t pageCount = buffer[offset++];
  if (pageCount > MAX_PAGES) return false;

  for (uint8_t page = 0; page < pageCount; page++) {
    if (offset + 1 > bufferLen) return false;
    uint8_t slots = buffer[offset++];
    if (slots > PRESETS_PER_BANK) return false;

    if (offset + (size_t)slots * 2 > bufferLen) return false;
    for (uint8_t slot = 0; slot < slots; slot++) {
      layout.presetIndex[page][slot] = buffer[offset] | (buffer[offset + 1] << 8);
      offset += 2;
    }
    layout.slotCount[page] = slots;
  }
  layout.pageCount = pageCount;

  return layout.refersOnlyTo(presetCount);
}

// Reads `count` preset records. Returns false when one is truncated.
bool parseRecords(
  const uint8_t* buffer, size_t bufferLen,
  uint16_t count,
  Preset* out, size_t& offset
) {
  for (uint16_t i = 0; i < count; i++) {
    if (offset + 2 > bufferLen) return false; // can't even read nameLen

    uint16_t nameLen = buffer[offset] | (buffer[offset + 1] << 8);
    offset += 2;

    if (offset + nameLen + PACKET2_LEN > bufferLen) return false; // truncated record

    uint16_t copyLen = nameLen;
    if (copyLen > MAX_PRESET_NAME_LEN) copyLen = MAX_PRESET_NAME_LEN; // defensive clamp

    memset(out[i].name, 0, sizeof(out[i].name));
    memcpy(out[i].name, buffer + offset, copyLen);
    offset += nameLen; // advance by the FULL declared length even if we clamped the copy

    memcpy(out[i].packet2, buffer + offset, PACKET2_LEN);
    offset += PACKET2_LEN;
  }
  return true;
}

} // namespace

bool parseTransferBuffer(
  const uint8_t* buffer, size_t bufferLen,
  uint16_t declaredCount,
  ParsedLibrary& out
) {
  if (declaredCount > MAX_LIBRARY_PRESETS) return false;

  size_t offset = 0;
  if (!parsePageTable(buffer, bufferLen, declaredCount, out.layout, offset)) return false;
  if (!parseRecords(buffer, bufferLen, declaredCount, out.presets, offset)) return false;

  // Leftover bytes mean the sender and this parser disagree on the format.
  if (offset != bufferLen) return false;

  out.presetCount = declaredCount;
  return true;
}
