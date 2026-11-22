#include "Settings.h"
#include <Preferences.h>

Settings settings;

namespace {
  Preferences prefs;
  const char* NAMESPACE = "pulzefs";
  const char* KEY_AMP_ADDR = "ampAddr";
  const char* KEY_AMP_ADDR_TYPE = "ampAddrType";
  const char* KEY_LAST_IDX = "lastIdx";
}

void Settings::begin() {
  prefs.begin(NAMESPACE, false);
}

String Settings::getAmpAddress() const {
  return prefs.getString(KEY_AMP_ADDR, "");
}

uint8_t Settings::getAmpAddressType() const {
  // Default of 1 (BLE_ADDR_RANDOM) rather than 0 (PUBLIC) - confirmed
  // via real hardware testing that the Pulze Mini advertises with a
  // random address. This only affects the very first guess on a
  // fresh/reset device; scanAndConnect() always learns and persists
  // the real type from an actual scan regardless, so this default
  // being wrong for some other unit would just cost one extra scan
  // fallback, not break anything.
  return prefs.getUChar(KEY_AMP_ADDR_TYPE, 1);
}

void Settings::setAmpAddress(const String& address, uint8_t addressType) {
  prefs.putString(KEY_AMP_ADDR, address);
  prefs.putUChar(KEY_AMP_ADDR_TYPE, addressType);
}

uint16_t Settings::getLastActiveIndex() const {
  return prefs.getUShort(KEY_LAST_IDX, 0xFFFF);
}

void Settings::setLastActiveIndex(uint16_t index) {
  prefs.putUShort(KEY_LAST_IDX, index);
}
