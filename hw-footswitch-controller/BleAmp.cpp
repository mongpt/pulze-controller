#include "BleAmp.h"
#include "config.h"
#include "Settings.h"
#include <NimBLEDevice.h>

BleAmp bleAmp;

namespace {

NimBLEClient* client = nullptr;
NimBLERemoteCharacteristic* dataChar = nullptr;

class ClientCallbacks : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient* c, int reason) override {
    Serial.printf("[BleAmp] disconnected (reason %d)\n", reason);
    dataChar = nullptr;
    bleAmp.onDisconnected();
  }
};

ClientCallbacks clientCallbacks;

} // namespace

void BleAmp::begin() {
  // Request the largest MTU up front - this is the one thing native
  // code can do that Web Bluetooth couldn't: packet2 is 194 bytes and
  // needs ATT_MTU >= 197 to go out as a single write. Safe to call any
  // time before actually connecting; applies as the preferred value
  // for whatever connection comes next.
  NimBLEDevice::setMTU(247);
}

uint8_t BleAmp::nextSeq() {
  uint8_t s = _seq;
  _seq++;
  if (_seq > 0x7f) _seq = 0x2c;
  return s;
}

bool BleAmp::connectToAddress(const NimBLEAddress& addr) {
  client = NimBLEDevice::createClient();
  client->setClientCallbacks(&clientCallbacks, false);

  if (!client->connect(addr)) {
    Serial.println("[BleAmp] connect failed");
    NimBLEDevice::deleteClient(client);
    client = nullptr;
    return false;
  }

  NimBLERemoteService* service = client->getService(MIDI_SERVICE_UUID);
  if (!service) {
    Serial.println("[BleAmp] amp has no BLE-MIDI service?!");
    client->disconnect();
    return false;
  }

  dataChar = service->getCharacteristic(MIDI_CHAR_UUID);
  if (!dataChar) {
    Serial.println("[BleAmp] amp's BLE-MIDI service has no data characteristic?!");
    client->disconnect();
    return false;
  }

  Serial.println("[BleAmp] connected");
  return true;
}

bool BleAmp::connectToKnownAddress(const String& address) {
  if (address.isEmpty()) return false;

  // Reconstructing an address from a bare string requires guessing its
  // type (public vs random) unless we stored it - which we now do (see
  // scanAndConnect()). A stale/never-set type here would fall back to
  // BLE_ADDR_PUBLIC (0), which is what caused this to silently fail
  // against a device using a different address type: the connect
  // attempt just fails outright with no more specific error, since
  // it's effectively trying to connect to an address of the wrong kind.
  uint8_t addrType = settings.getAmpAddressType();
  Serial.printf("[BleAmp] connecting to known address %s (type %u)...\n", address.c_str(), addrType);
  NimBLEAddress addr(std::string(address.c_str()), addrType);
  return connectToAddress(addr);
}

bool BleAmp::scanAndConnect() {
  Serial.println("[BleAmp] scanning for a device advertising the BLE-MIDI service...");

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  NimBLEScanResults results = scan->getResults(5000, false); // 5s scan, blocking

  Serial.printf("[BleAmp] scan finished, %d device(s) seen:\n", results.getCount());

  for (int i = 0; i < results.getCount(); i++) {
    const NimBLEAdvertisedDevice* device = results.getDevice(i);
    bool hasMidiService = device->isAdvertisingService(NimBLEUUID(MIDI_SERVICE_UUID));

    Serial.printf("  [%d] %s  name=\"%s\"  serviceCount=%d  hasMidiService=%s\n",
                  i,
                  device->getAddress().toString().c_str(),
                  device->getName().c_str(),
                  device->getServiceUUIDCount(),
                  hasMidiService ? "YES" : "no");

    for (int s = 0; s < device->getServiceUUIDCount(); s++) {
      Serial.printf("        service[%d] = %s\n", s, device->getServiceUUID(s).toString().c_str());
    }

    if (hasMidiService) {
      // Use the address exactly as the scan reported it - including
      // its real type - rather than rebuilding it from a plain string
      // and guessing the type. This is the actual fix: the previous
      // version threw the type away and always guessed PUBLIC, which
      // silently failed to connect whenever that guess was wrong.
      NimBLEAddress addr = device->getAddress();
      String address = addr.toString().c_str();
      uint8_t addrType = addr.getType();

      Serial.printf("[BleAmp] found %s [%s] type=%u\n",
                    device->getName().c_str(), address.c_str(), addrType);
      settings.setAmpAddress(address, addrType);
      return connectToAddress(addr);
    }
  }

  Serial.println("[BleAmp] no matching device found");
  return false;
}

bool BleAmp::connect() {
  if (_state == AmpConnState::CONNECTED) {
    Serial.println("[BleAmp] connect() called while already connected, ignoring");
    return true;
  }

  _state = AmpConnState::CONNECTING;

  String knownAddress = settings.getAmpAddress();
  bool ok = connectToKnownAddress(knownAddress);

  if (!ok) {
    _state = AmpConnState::SCANNING;
    ok = scanAndConnect();
  }

  _state = ok ? AmpConnState::CONNECTED : AmpConnState::DISCONNECTED;
  return ok;
}

void BleAmp::disconnect() {
  if (client) {
    client->disconnect(); // triggers ClientCallbacks::onDisconnect -> onDisconnected()
  } else {
    _state = AmpConnState::DISCONNECTED;
  }
}

void BleAmp::onDisconnected() {
  _state = AmpConnState::DISCONNECTED;
}

bool BleAmp::sendPreset(const Preset* preset) {
  if (_state != AmpConnState::CONNECTED || !dataChar) {
    Serial.println("[BleAmp] sendPreset called while not connected");
    return false;
  }

  uint8_t seq = nextSeq();

  uint8_t p2[PACKET2_LEN];
  memcpy(p2, preset->packet2, PACKET2_LEN);
  p2[7] = seq;

  uint8_t p3[PACKET3_LEN];
  memcpy(p3, PACKET3_TEMPLATE, PACKET3_LEN);
  p3[7] = seq;

  bool ok = true;
  ok &= dataChar->writeValue(p2, PACKET2_LEN, false); // false = write without response
  delay(PRESET_SEND_GAP_1_MS);
  ok &= dataChar->writeValue(p3, PACKET3_LEN, false);
  delay(PRESET_SEND_GAP_2_MS);
  ok &= dataChar->writeValue((uint8_t*)PACKET_COMMIT, sizeof(PACKET_COMMIT), false);

  Serial.printf("[BleAmp] sent preset \"%s\": %s\n", preset->name, ok ? "ok" : "FAILED");
  return ok;
}
