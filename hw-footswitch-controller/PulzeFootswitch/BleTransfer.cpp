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
// phone/app is currently connected (so the Connect footswitch can
// require that connection be freed first).
// ---------------------------------------------------------------------

#include "BleTransfer.h"
#include "config.h"
#include <NimBLEDevice.h>

BleTransfer bleTransfer;

namespace {

NimBLEServer* server = nullptr;
NimBLECharacteristic* dataChar = nullptr;
// ESP32 core 3/4 builds NimBLE with extended advertising, which removes
// NimBLEAdvertising. The packet stays a legacy connectable advert so the
// browser can still filter on the service UUID.
#if CONFIG_BT_NIMBLE_EXT_ADV
NimBLEExtAdvertising* advertising = nullptr;
const uint8_t ADVERT_INSTANCE = 0;
#else
NimBLEAdvertising* advertising = nullptr;
#endif

const size_t RX_BUFFER_CAP = 64 * 1024;
uint8_t* rxBuffer = nullptr;
size_t rxLen = 0;
uint16_t expectedCount = 0;
bool transferInProgress = false;

const char* DEVICE_NAME = "Pulze Footswitch";

bool startAdvertising() {
  if (!advertising) return false;

#if CONFIG_BT_NIMBLE_EXT_ADV
  // Flags plus the 128-bit UUID use 21 of the 31 legacy advert bytes.
  // The name goes in the scan response so the browser picker can show it.
  NimBLEExtAdvertisement advert;
  advert.setLegacyAdvertising(true);
  advert.setConnectable(true);
  advert.setFlags(BLE_HS_ADV_F_DISC_GEN);
  if (!advert.addServiceUUID(FOOTSWITCH_SERVICE_UUID)) {
    Serial.println("[BleTransfer] service UUID does not fit in the advert");
    return false;
  }

  NimBLEExtAdvertisement scanResponse;
  if (!scanResponse.setName(DEVICE_NAME)) {
    Serial.println("[BleTransfer] device name does not fit in the scan response");
    return false;
  }

  if (!advertising->setInstanceData(ADVERT_INSTANCE, advert)) {
    Serial.println("[BleTransfer] failed to set advert data");
    return false;
  }
  if (!advertising->setScanResponseData(ADVERT_INSTANCE, scanResponse)) {
    Serial.println("[BleTransfer] failed to set scan response");
    return false;
  }
  return advertising->start(ADVERT_INSTANCE);
#else
  advertising->setName(DEVICE_NAME);
  return advertising->start();
#endif
}

void stopAdvertising() {
  if (!advertising) return;
#if CONFIG_BT_NIMBLE_EXT_ADV
  advertising->stop(ADVERT_INSTANCE);
#else
  advertising->stop();
#endif
}

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
    startAdvertising();
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

        static ParsedLibrary parsed; // static: too large for the callback's stack

        if (!parseTransferBuffer(rxBuffer, rxLen, expectedCount, parsed)) {
          Serial.println("[BleTransfer] parse FAILED - transfer rejected, keeping existing library");
        } else {
          bool ok = presetStore.replaceAll(parsed.presets, parsed.presetCount, parsed.layout);
          Serial.printf("[BleTransfer] library replaced with %u preset(s) on %u page(s): %s\n",
                        parsed.presetCount, parsed.layout.pageCount,
                        ok ? "saved" : "SAVE FAILED");
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
#if !CONFIG_BT_NIMBLE_EXT_ADV
  advertising->addServiceUUID(FOOTSWITCH_SERVICE_UUID);
#endif
  if (!startAdvertising()) {
    Serial.println("[BleTransfer] failed to start advertising");
    return;
  }

  Serial.println("[BleTransfer] advertising as 'Pulze Footswitch'");
}

void BleTransfer::pauseAdvertising() {
  stopAdvertising();
}

void BleTransfer::resumeAdvertising() {
  // Rebuild the payload before every resume. stop()/start() can drop
  // the name, and the browser then shows an opaque device id.
  if (!startAdvertising()) {
    Serial.println("[BleTransfer] failed to resume advertising");
  }
}
