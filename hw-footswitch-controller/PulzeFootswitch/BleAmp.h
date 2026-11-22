// ---------------------------------------------------------------------
// BleAmp.h - BLE CENTRAL role: connects OUT to the Pulze amp (same role
// the PC/phone tools have always played), sends the packet2+packet3+
// commit sequence to recall an exact preset.
//
// Connection is BUTTON-DRIVEN, not automatic: this device also runs a
// BLE peripheral (BleTransfer) continuously from boot so the web app
// can connect for a preset-library transfer at any time. Rather than
// betting on true simultaneous central+peripheral connections working
// flawlessly on this specific chip/library combo (unverified from
// here), only one role is ever actually CONNECTED at a time - the
// dedicated Connect footswitch connects to the amp (refusing if the
// app is currently attached - see BleTransfer::isAppConnected()) or
// disconnects from it if already connected. See PulzeFootswitch.ino
// for the button handling.
//
// Also owns the tuner state (MIDI CC 55), mirroring the web app's
// Tuner tile: sending any preset turns the tuner off first, and a BLE
// disconnect clears it, since the amp forgets it across connections.
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

  // Triggered by the Connect footswitch, not automatic - see the design
  // note in PulzeFootswitch.ino for why connection is button-driven
  // rather than always-on in the background.
  bool connect();       // tries known address first, falls back to a scan
  void disconnect();

  // Sends the full recall sequence for this preset - same 100ms/300ms
  // gaps as the Python/web versions, since that's inherent to how the
  // amp processes a full patch load. If the tuner is on it is turned
  // off first, same as tapping a patch in the web app.
  bool sendPreset(const Preset* preset);

  // Tuner on/off via MIDI CC 55 (TUNER_CC in config.h).
  bool setTuner(bool on);
  bool isTunerOn() const { return _tunerOn; }

  void onDisconnected(); // called by the NimBLE client callback

private:
  AmpConnState _state = AmpConnState::DISCONNECTED;
  uint8_t _seq = 0x2c;
  bool _tunerOn = false;

  bool connectToKnownAddress(const String& address);
  bool scanAndConnect();
  bool connectToAddress(const NimBLEAddress& addr); // shared by both paths above
  bool sendControlChange(uint8_t cc, uint8_t value); // channel 0
  uint8_t nextSeq();
};

extern BleAmp bleAmp;
