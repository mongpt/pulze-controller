// ---------------------------------------------------------------------
// TransferParser.cpp - pure logic, no BLE/hardware dependencies.
//
// This file intentionally does not touch NimBLE, LittleFS, or Serial -
// it only reads bytes out of a buffer and writes Preset structs. That's
// what makes it possible to compile and run this exact logic on a
// desktop machine (see host_test/) and prove it's correct before it
// ever runs on real hardware.
// ---------------------------------------------------------------------

#include "BleTransfer.h"
#include <cstring>

int parseTransferBuffer(
  const uint8_t* buffer, size_t bufferLen,
  uint16_t count,
  Preset* out, uint16_t maxOut
) {
  size_t offset = 0;

  for (uint16_t i = 0; i < count; i++) {
    if (i >= maxOut) return -1; // more presets than the caller has room for

    if (offset + 2 > bufferLen) return -1; // truncated: can't even read nameLen

    uint16_t nameLen = buffer[offset] | (buffer[offset + 1] << 8);
    offset += 2;

    if (offset + nameLen + PACKET2_LEN > bufferLen) return -1; // truncated record

    uint16_t copyLen = nameLen;
    if (copyLen > MAX_PRESET_NAME_LEN) copyLen = MAX_PRESET_NAME_LEN; // defensive clamp

    memset(out[i].name, 0, sizeof(out[i].name));
    memcpy(out[i].name, buffer + offset, copyLen);
    offset += nameLen; // advance by the FULL declared length even if we clamped the copy

    memcpy(out[i].packet2, buffer + offset, PACKET2_LEN);
    offset += PACKET2_LEN;
  }

  return (int)count;
}
