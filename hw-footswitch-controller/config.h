// ---------------------------------------------------------------------
// config.h - pin map, BLE identifiers, and shared constants.
//
// The BLE UUIDs here are NOT arbitrary - they must exactly match:
//  - MIDI_SERVICE_UUID / MIDI_CHAR_UUID: the official BLE-MIDI spec
//    UUIDs, identical on the Pulze amp and every BLE-MIDI device.
//  - FOOTSWITCH_SERVICE_UUID / FOOTSWITCH_CHAR_UUID: the custom UUIDs
//    already hardcoded in the web app's app.js. If these ever change,
//    they must change in BOTH places together.
// ---------------------------------------------------------------------

#pragma once

#include <Arduino.h>

// ---- pin map (ESP32-S3-N16R8) -----------------------------------------
// See project notes: avoids strapping pins (0,3,45,46), the octal
// PSRAM/flash pins reserved on the N16R8 variant (26-37), native USB
// D-/D+ (19,20), and the onboard RGB LED most S3 DevKitC boards wire
// to GPIO48. Verify against your specific board's silkscreen.

#define PIN_OLED_SDA      8
#define PIN_OLED_SCL      9

#define PIN_FW1           4   // preset 1 (bank offset 0)
#define PIN_FW2           5   // preset 2 (bank offset 1)
#define PIN_FW3           6   // preset 3 (bank offset 2)
#define PIN_FW4           7   // preset 4 (bank offset 3)
#define PIN_BANK_UP       15
#define PIN_BANK_DOWN     16

#define PIN_LED1          17
#define PIN_LED2          18
#define PIN_LED3          39
#define PIN_LED4          40

// ---- BLE: talking to the amp (central role) ---------------------------
// Official BLE-MIDI UUIDs - identical on every BLE-MIDI device.
#define MIDI_SERVICE_UUID   "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define MIDI_CHAR_UUID      "7772e5db-3868-4112-a1a9-f2669d106bf3"

// ---- BLE: talking to the web app (peripheral role, transfer mode) -----
// Custom, made-up UUIDs - must match FOOTSWITCH_SERVICE_UUID /
// FOOTSWITCH_CHAR_UUID in the web app's app.js exactly.
#define FOOTSWITCH_SERVICE_UUID  "e6f80001-b5f0-4eea-9a1e-31b0d6cfa930"
#define FOOTSWITCH_CHAR_UUID     "e6f80002-b5f0-4eea-9a1e-31b0d6cfa930"

// ---- amp protocol constants (same bytes as the Python/web versions) ---
#define PACKET2_LEN       194
#define PACKET3_LEN       88

// Shared across every preset - same as PACKET3_TEMPLATE_HEX in app.js.
// Generated directly from that hex string (not hand-transcribed) to
// avoid transcription errors - verified byte-for-byte against it.
static const uint8_t PACKET3_TEMPLATE[PACKET3_LEN] = {
  0x8f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x0e,
  0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x01, 0x0b,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x03, 0x05,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x01, 0x02,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0xd6, 0xf7
};

// Fixed trailer sent after packet2+packet3 - same as PACKET_COMMIT_HEX.
static const uint8_t PACKET_COMMIT[16] = {
  0x80,0x80,0xf0,0x21,0x25,0x41,0x50,0x00,
  0x00,0x02,0x14,0x04,0x01,0x04,0x01,0xf7
};

// ---- footswitch controller constants -----------------------------------
#define PRESETS_PER_BANK      4
#define MAX_PRESET_NAME_LEN   40   // truncate anything longer on receive
#define PRESETS_FILE_PATH     "/presets.bin"

// ---- transfer protocol (v2) --------------------------------------------
// Every BLE write during a transfer starts with one of these type tags
// as its very first byte - this is what makes control messages
// unambiguous from data, since data payload bytes are arbitrary preset
// content with no restriction on what values they can contain. (v1 of
// this protocol tried to use a magic byte value instead of an explicit
// tag and had a real, if rare, ambiguity bug - a data chunk could
// coincidentally start with the magic byte and get misread as END.)
//
//   START: [FS_MSG_START, countLo, countHi]   (3 bytes total)
//   DATA:  [FS_MSG_DATA, ...up to 17 payload bytes]  (<= 18 bytes total)
//   END:   [FS_MSG_END]                        (1 byte total)
//
// Must match FS_MSG_* / the chunking logic in the web app's app.js
// exactly - if either side changes, both must change together.
#define FS_MSG_START          0x01
#define FS_MSG_DATA           0x02
#define FS_MSG_END            0x03
#define FS_MSG_MAX_WRITE_LEN  18   // total bytes per write, tag included

#define DEBOUNCE_MS           30
#define LONG_PRESS_MS         2000  // hold Bank Up + Bank Down this long at boot -> transfer mode
#define LAST_USED_SETTLE_MS   5000  // wait this long with no further FW change before persisting
                                     // "last used preset" to NVS - avoids a write on every single
                                     // press during rapid switching. Sending to the amp itself is
                                     // NOT delayed by this - every press still plays instantly.

// Same 100ms/300ms gaps as the Python/web versions - this is inherent
// to how the amp processes a full patch load, not a firmware choice.
#define PRESET_SEND_GAP_1_MS  100
#define PRESET_SEND_GAP_2_MS  300
