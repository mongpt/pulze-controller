// ---------------------------------------------------------------------
// BleTransfer.cpp - the NimBLE plumbing around parseTransferBuffer().
//
// All the tricky logic (parsing the received bytes into Presets) lives
// in TransferParser.cpp and is unit-tested on a desktop compiler - see
// host_test/. This file's only job is: advertise, accept writes,
// strip the message-type tag, accumulate DATA bytes, and call the
// tested parser when END arrives.
// ---------------------------------------------------------------------

// ---------------------------------------------------------------------
// BleTransfer.cpp - the NimBLE plumbing around parseTransferBuffer().
//
// All the tricky logic (parsing the received bytes into Presets) lives
// in TransferParser.cpp and is unit-tested on a desktop compiler - see
// host_test/. This file's job is: advertise continuously from boot,
// accept writes, strip the message-type tag, accumulate DATA bytes,
// call the tested parser when END arrives, and track whether a
// phone/app is currently connected (so the amp-connect gesture can
// require that connection be freed first).
// ---------------------------------------------------------------------

#include "BleTransfer.h"
#include "config.h"
#include <NimBLEDevice.h>

BleTransfer bleTransfer;

namespace {

NimBLEServer* server = nullptr;
NimBLECharacteristic* dataChar = nullptr;
NimBLEAdvertising* advertising = nullptr;

const size_t RX_BUFFER_CAP = 64 * 1024;
uint8_t* rxBuffer = nullptr;
size_t rxLen = 0;
uint16_t expectedCount = 0;
bool transferInProgress = false;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
    bleTransfer._appConnected = true;
    Serial.println("[BleTransfer] app connected");
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
    bleTransfer._appConnected = false;
    transferInProgress = false;
    bleTransfer._receiving = false;
    rxLen = 0;
    Serial.println("[BleTransfer] app disconnected");
    // Resume advertising automatically so the app can reconnect later -
    // harmless no-op if we're mid-amp-connection and deliberately paused,
    // since pauseAdvertising() will have been called after this anyway
    // in that flow's ordering.
    if (advertising) advertising->start();
  }
};

class DataCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
    std::string value = c->getValue();
    if (value.empty()) return;

    uint8_t tag = (uint8_t)value[0];

    switch (tag) {
      case FS_MSG_START: {
        if (value.size() < 3) {
          Serial.println("[BleTransfer] malformed START, ignoring");
          return;
        }
        expectedCount = (uint8_t)value[1] | ((uint8_t)value[2] << 8);
        rxLen = 0;
        transferInProgress = true;
        bleTransfer._receiving = true;
        Serial.printf("[BleTransfer] START, expecting %u preset(s)\n", expectedCount);
        break;
      }

      case FS_MSG_DATA: {
        if (!transferInProgress) {
          Serial.println("[BleTransfer] DATA received with no active transfer, ignoring");
          return;
        }
        size_t payloadLen = value.size() - 1; // everything after the tag byte
        if (rxLen + payloadLen > RX_BUFFER_CAP) {
          Serial.println("[BleTransfer] transfer exceeds buffer capacity, aborting");
          transferInProgress = false;
          bleTransfer._receiving = false;
          return;
        }
        memcpy(rxBuffer + rxLen, value.data() + 1, payloadLen);
        rxLen += payloadLen;
        break;
      }

      case FS_MSG_END: {
        if (!transferInProgress) {
          Serial.println("[BleTransfer] END received with no active transfer, ignoring");
          return;
        }
        Serial.printf("[BleTransfer] END, accumulated %u bytes for %u preset(s)\n",
                       (unsigned)rxLen, expectedCount);

        static Preset parsed[256]; // matches PresetStore::MAX_PRESETS
        int n = parseTransferBuffer(rxBuffer, rxLen, expectedCount, parsed, 256);

        if (n < 0) {
          Serial.println("[BleTransfer] parse FAILED - transfer rejected, keeping existing library");
        } else {
          bool ok = presetStore.replaceAll(parsed, (uint16_t)n);
          Serial.printf("[BleTransfer] library replaced with %d preset(s): %s\n",
                        n, ok ? "saved" : "SAVE FAILED");
        }

        transferInProgress = false;
        bleTransfer._receiving = false;
        bleTransfer._complete = true; // main loop notices this and can show a "done" message
        break;
      }

      default:
        Serial.printf("[BleTransfer] unknown message tag 0x%02x, ignoring\n", tag);
    }
  }
};

ServerCallbacks serverCallbacks;
DataCallbacks dataCallbacks;

} // namespace

void BleTransfer::begin() {
  rxBuffer = (uint8_t*)malloc(RX_BUFFER_CAP);
  if (!rxBuffer) {
    Serial.println("[BleTransfer] FATAL: could not allocate receive buffer");
    return;
  }

  server = NimBLEDevice::createServer();
  server->setCallbacks(&serverCallbacks);

  NimBLEService* service = server->createService(FOOTSWITCH_SERVICE_UUID);
  dataChar = service->createCharacteristic(
    FOOTSWITCH_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE
  );
  dataChar->setCallbacks(&dataCallbacks);
  service->start();

  advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(FOOTSWITCH_SERVICE_UUID);
  // Explicit, deliberate call - don't rely on NimBLEDevice::init()'s
  // name alone reaching the actual over-the-air advertisement payload.
  // Confirmed on real hardware: without this, the connection itself
  // works fine (service UUID filtering finds and connects to the
  // device correctly), but no name is ever broadcast, so Chrome falls
  // back to showing an opaque device ID and generic scanners show
  // "Unknown device" - purely cosmetic, but worth fixing so the device
  // is actually identifiable in the connection dialog.
  advertising->setName("Pulze Footswitch");
  advertising->start();

  Serial.println("[BleTransfer] advertising as 'Pulze Footswitch'");
}

void BleTransfer::pauseAdvertising() {
  if (advertising) advertising->stop();
}

void BleTransfer::resumeAdvertising() {
  if (advertising) {
    // Re-assert the name defensively before every resume, in case
    // stop()/start() resets advertisement config in this NimBLE
    // version - cheap insurance against the name silently dropping
    // out after an amp-connect/disconnect cycle.
    advertising->setName("Pulze Footswitch");
    advertising->start();
  }
}
