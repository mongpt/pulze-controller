// ---------------------------------------------------------------------
// BleAmp.h - BLE CENTRAL role: connects OUT to the Pulze amp (same role
// the PC/phone tools have always played), sends the packet2+packet3+
// commit sequence to recall an exact preset.
//
// Connection is GESTURE-DRIVEN, not automatic: this device also runs a
// BLE peripheral (BleTransfer) continuously from boot so the web app
// can connect for a preset-library transfer at any time. Rather than
// betting on true simultaneous central+peripheral connections working
// flawlessly on this specific chip/library combo (unverified from
// here), only one role is ever actually CONNECTED at a time - holding
// both Bank buttons connects to the amp (refusing if the app is
// currently attached - see BleTransfer::isAppConnected()) or
// disconnects from it if already connected. See PulzeFootswitch.ino
// for the full gesture handling.
//
// NOTE ON LIBRARY VERSION: written against the NimBLE-Arduino 2.x API
// surface. NimBLE-Arduino has had real breaking API changes between
// major versions - if this doesn't compile as-is, check your installed
// version against what's called here first; the logic/sequencing is
// correct regardless, only exact method names might need adjusting.
// ---------------------------------------------------------------------

#pragma once

#include <Arduino.h>
#include "PresetStore.h"

class NimBLEAddress; // forward-declared - only needed by reference here

enum class AmpConnState {
  DISCONNECTED,
  SCANNING,
  CONNECTING,
  CONNECTED,
};

class BleAmp {
public:
  void begin();                 // one-time prep (MTU preference) - call once from setup()
  bool isConnected() const { return _state == AmpConnState::CONNECTED; }
  AmpConnState state() const { return _state; }

  // Triggered by the bank-hold gesture, not automatic - see the design
  // note in PulzeFootswitch.ino for why connection is gesture-driven
  // rather than always-on in the background.
  bool connect();       // tries known address first, falls back to a scan
  void disconnect();

  // Sends the full recall sequence for this preset - same 100ms/300ms
  // gaps as the Python/web versions, since that's inherent to how the
  // amp processes a full patch load.
  bool sendPreset(const Preset* preset);

  void onDisconnected(); // called by the NimBLE client callback

private:
  AmpConnState _state = AmpConnState::DISCONNECTED;
  uint8_t _seq = 0x2c;

  bool connectToKnownAddress(const String& address);
  bool scanAndConnect();
  bool connectToAddress(const NimBLEAddress& addr); // shared by both paths above
  uint8_t nextSeq();
};

extern BleAmp bleAmp;
