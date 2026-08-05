// ---------------------------------------------------------------------
// BleTransfer.h - BLE PERIPHERAL role: advertises FOOTSWITCH_SERVICE_UUID
// continuously from boot so the web app can find and connect to this
// device at any time, receives a full preset library over the v2
// chunked protocol (see config.h), and saves it into PresetStore on
// completion.
//
// Coexists with BleAmp's central-role connection to the amp under one
// shared NimBLEDevice::init() call, but the two are designed to never
// be CONNECTED at the same time - see the design note at the top of
// PulzeFootswitch.ino for why.
// ---------------------------------------------------------------------

#pragma once

#include <Arduino.h>
#include "PresetStore.h"

// Pure parsing logic, exposed separately from the BLE plumbing so it
// can be unit-tested on a desktop compiler without any BLE stack or
// hardware involved - see host_test/test_transfer_parser.cpp.
//
// Parses `buffer` (the raw accumulated DATA payload bytes, in receive
// order) as exactly `count` back-to-back records of
// [nameLen u16 LE][name bytes][194-byte packet2], writing up to
// `maxOut` results into `out`. Returns the number of presets actually
// parsed, or -1 on a malformed/truncated buffer.
int parseTransferBuffer(
  const uint8_t* buffer, size_t bufferLen,
  uint16_t count,
  Preset* out, uint16_t maxOut
);

class BleTransfer {
public:
  void begin();               // starts the peripheral (server + advertising) - call once at boot
  bool isTransferComplete() const { return _complete; }
  void clearTransferComplete() { _complete = false; }

  // True while a phone/app is currently connected to this device's
  // peripheral service - the amp-connect gesture checks this and
  // refuses to proceed until it's false, so the app's connection is
  // explicitly freed first rather than betting on true simultaneous
  // dual-role operation.
  bool isAppConnected() const { return _appConnected; }

  // Suspends/resumes advertising (does not tear down the server) -
  // used while BleAmp's central-role connection to the amp is active,
  // per the "one role actually connected at a time" design.
  void pauseAdvertising();
  void resumeAdvertising();

  // True while actively receiving a transfer (between START and END) -
  // lets the UI show a dedicated "receiving" message instead of the
  // normal performance display.
  bool isReceiving() const { return _receiving; }

  bool _complete = false;
  bool _appConnected = false;
  bool _receiving = false;
};

extern BleTransfer bleTransfer;
