// ---------------------------------------------------------------------
// PulzeFootswitch.ino - main entry point.
//
// ARCHITECTURE: the BLE peripheral (BleTransfer, for the web app) runs
// continuously from boot - the web app can connect and transfer a
// library at any time, no special mode needed on the footswitch side
// for that. The BLE central connection to the amp (BleAmp) is
// GESTURE-DRIVEN instead: hold both Bank buttons together to connect,
// same gesture again to disconnect.
//
// Both layer on top of a single NimBLEDevice::init() call made once in
// setup() - this is standard, well-supported NimBLE usage (a server
// and a client coexisting under one BLE stack instance). What's
// deliberately NOT attempted is having BOTH a live peripheral
// connection (to the app) AND a live central connection (to the amp)
// AT THE SAME TIME - the amp-connect gesture refuses to proceed while
// the app is attached, requiring it be disconnected first (via the
// app's own "Disconnect Footswitch" button). This is a conservative
// choice: true simultaneous dual-role operation may well work fine on
// this chip, but it's unverified from here, and one-role-connected-
// at-a-time is simple to reason about and guaranteed not to have
// concurrency surprises.
//
// Two FreeRTOS tasks:
//   - uiTask (core 1): buttons, LEDs, display, bank/preset navigation,
//     the gesture, everything user-facing.
//   - bleTask (core 0): idles on a queue, handles CONNECT_AMP /
//     DISCONNECT_AMP / PLAY_PRESET requests from uiTask. Runs on its
//     own core so a preset send (~400-500ms) or a scan-and-connect
//     (several seconds) never blocks button reading or the display.
// ---------------------------------------------------------------------

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "config.h"
#include "PresetStore.h"
#include "Settings.h"
#include "Buttons.h"
#include "Display.h"
#include "BleAmp.h"
#include "BleTransfer.h"

enum class BleRequestType { CONNECT_AMP, DISCONNECT_AMP, PLAY_PRESET };
struct BleRequest {
  BleRequestType type;
  uint16_t presetIndex;
};

static QueueHandle_t bleRequestQueue;

// Recomputes bank/offset/haveActive from settings' remembered index,
// bounds-checked against the CURRENT library. Used both at boot and
// after a transfer replaces the library (where the old in-memory
// bank/offset could otherwise point at a bank/preset that no longer
// exists, or no longer means what it used to, in the new library).
static void loadActivePresetState(uint16_t& bank, uint8_t& activeOffset, bool& haveActive) {
  uint16_t lastIdx = settings.getLastActiveIndex();
  if (lastIdx != 0xFFFF && presetStore.count() > 0 && lastIdx < presetStore.count()) {
    bank = lastIdx / PRESETS_PER_BANK;
    activeOffset = lastIdx % PRESETS_PER_BANK;
    haveActive = true;
  } else {
    bank = 0;
    activeOffset = 0;
    haveActive = false;
  }
}

// ---- BLE task (core 0) --------------------------------------------------

static void bleTaskFn(void* /*param*/) {
  bleAmp.begin();

  BleRequest req;
  for (;;) {
    if (xQueueReceive(bleRequestQueue, &req, portMAX_DELAY) != pdTRUE) continue;

    switch (req.type) {
      case BleRequestType::CONNECT_AMP: {
        bleTransfer.pauseAdvertising();
        bool ok = bleAmp.connect();
        if (!ok) {
          Serial.println("[main] amp connect failed, resuming app advertising");
          bleTransfer.resumeAdvertising();
        } else {
          // Immediately re-apply whatever was last active, so the amp
          // ends up in the expected state right away rather than
          // waiting for the next footswitch press - the amp has no
          // memory of what the footswitch last told it across a BLE
          // disconnect, so without this it could be sitting on
          // whatever it was doing before (someone else's edits, a
          // different patch entirely, etc).
          uint16_t lastIdx = settings.getLastActiveIndex();
          if (lastIdx != 0xFFFF && lastIdx < presetStore.count()) {
            const Preset* p = presetStore.get(lastIdx);
            if (p) {
              Serial.println("[main] re-applying last-used preset after connect");
              bleAmp.sendPreset(p);
            }
          }
        }
        break;
      }
      case BleRequestType::DISCONNECT_AMP: {
        bleAmp.disconnect();
        bleTransfer.resumeAdvertising();
        break;
      }
      case BleRequestType::PLAY_PRESET: {
        const Preset* p = presetStore.get(req.presetIndex);
        if (p) bleAmp.sendPreset(p);
        break;
      }
    }
  }
}

// ---- UI task (core 1) ----------------------------------------------------

static void uiTaskFn(void* /*param*/) {
  buttons.begin();

  if (!display.begin()) {
    Serial.println("[main] display init failed - continuing without it");
  }
  display.showBoot();
  delay(1000);

  uint16_t bank = 0;
  uint8_t activeOffset = 0;
  bool haveActive = false;
  loadActivePresetState(bank, activeOffset, haveActive);
  // Tracks which bank the active preset actually lives in, updated
  // instantly on every press - deliberately NOT derived from
  // settings.getLastActiveIndex() during the session, since that NVS
  // write is now debounced (see LAST_USED_SETTLE_MS) and would be
  // stale for up to LAST_USED_SETTLE_MS after a press. NVS is only the
  // source of truth at boot; during a running session, this in-memory value is.
  uint16_t activeBank = bank;
  if (haveActive) buttons.setLed(activeOffset, true);

  const ButtonId fwButtons[4] = { ButtonId::FW1, ButtonId::FW2, ButtonId::FW3, ButtonId::FW4 };

  bool showingGestureRefusal = false;
  unsigned long gestureRefusalUntil = 0;

  // "Last used" NVS writes are debounced separately from actually
  // playing a preset - every press still plays instantly (see the
  // BleRequest send below, unaffected by this), but the value only
  // gets PERSISTED to flash after LAST_USED_SETTLE_MS has passed with
  // no further footswitch change, so rapid switching doesn't write to
  // NVS on every single press or leave "last used" pointing at
  // something only briefly passed through.
  uint16_t pendingLastIdx = 0xFFFF;
  unsigned long pendingLastIdxSince = 0;

  for (;;) {
    buttons.update();

    // While actively receiving a transfer, show a dedicated message and
    // skip normal footswitch/bank handling entirely - the amp can't be
    // connected right now anyway (the gesture already refuses to
    // connect while the app is attached), so there's nothing useful for
    // FW/bank presses to do until this finishes.
    if (bleTransfer.isReceiving()) {
      display.showReceiving();
      delay(20);
      continue;
    }

    uint16_t totalBanks = presetStore.totalBanks();

    // Bank navigation - ignore a bank button's own press if the OTHER
    // bank button is also currently held, since that combination means
    // the user is forming the two-button gesture, not navigating.
    if (totalBanks > 0 && buttons.wasPressed(ButtonId::BANK_UP) && !buttons.isHeld(ButtonId::BANK_DOWN)) {
      bank = (bank + 1) % totalBanks;
    }
    if (totalBanks > 0 && buttons.wasPressed(ButtonId::BANK_DOWN) && !buttons.isHeld(ButtonId::BANK_UP)) {
      bank = (bank == 0) ? (totalBanks - 1) : (bank - 1);
    }

    // Preset footswitches - play immediately, but only mark this as a
    // CANDIDATE for "last used" (see the settle check below), not an
    // immediate NVS write.
    for (uint8_t i = 0; i < 4; i++) {
      if (buttons.wasPressed(fwButtons[i])) {
        const Preset* p = presetStore.getByBankAndOffset(bank, i);
        if (p) {
          uint16_t idx = (uint16_t)bank * PRESETS_PER_BANK + i;

          BleRequest req{ BleRequestType::PLAY_PRESET, idx };
          xQueueSend(bleRequestQueue, &req, 0);

          buttons.allLedsOff();
          buttons.setLed(i, true);
          activeOffset = i;
          activeBank = bank;
          haveActive = true;

          pendingLastIdx = idx;
          pendingLastIdxSince = millis();
        }
      }
    }

    // Amp connect/disconnect gesture
    if (buttons.bankGestureTriggered()) {
      if (bleAmp.isConnected()) {
        BleRequest req{ BleRequestType::DISCONNECT_AMP, 0 };
        xQueueSend(bleRequestQueue, &req, 0);
      } else if (bleTransfer.isAppConnected()) {
        Serial.println("[main] amp-connect gesture ignored - app is currently connected");
        showingGestureRefusal = true;
        gestureRefusalUntil = millis() + 2000;
      } else {
        BleRequest req{ BleRequestType::CONNECT_AMP, 0 };
        xQueueSend(bleRequestQueue, &req, 0);
      }
    }

    // Commit "last used" to NVS once it's settled (no further
    // footswitch change for LAST_USED_SETTLE_MS).
    if (pendingLastIdx != 0xFFFF && (millis() - pendingLastIdxSince) >= LAST_USED_SETTLE_MS) {
      settings.setLastActiveIndex(pendingLastIdx);
      Serial.printf("[main] last-used preset settled and saved: index %u\n", pendingLastIdx);
      pendingLastIdx = 0xFFFF;
    }

    // A transfer just finished - show a brief confirmation, then reset
    // the display back to the default "<bank>-<preset name>" view.
    // Recomputed fresh (not just resumed from whatever bank/offset were
    // showing before the transfer) since the just-received library may
    // be a completely different size/order than what was there before -
    // the old in-memory bank/offset could otherwise point at something
    // that no longer means what it used to, or doesn't exist at all.
    if (bleTransfer.isTransferComplete()) {
      display.showTransferDone(presetStore.count());
      bleTransfer.clearTransferComplete();
      delay(2000);

      loadActivePresetState(bank, activeOffset, haveActive);
      activeBank = bank;
      buttons.allLedsOff();
      if (haveActive) buttons.setLed(activeOffset, true);
    }

    if (showingGestureRefusal && millis() < gestureRefusalUntil) {
      display.showError("Disconnect the app\nfirst (use the\nfootswitch button\nin the web app)");
    } else {
      showingGestureRefusal = false;

      const Preset* shown = haveActive ? presetStore.getByBankAndOffset(bank, activeOffset) : nullptr;
      bool activeIsInThisBank = haveActive && (bank == activeBank);

      display.showPerformance(
        bleAmp.state(),
        bank,
        activeIsInThisBank ? activeOffset : 0,
        totalBanks,
        activeIsInThisBank && shown ? shown->name : ""
      );
    }

    delay(10);
  }
}

// ---- Arduino entry points -------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[main] Pulze Footswitch starting");

  settings.begin();
  presetStore.begin();

  // One shared BLE stack init - both the peripheral (BleTransfer) and
  // the central (BleAmp) layer on top of this single call.
  NimBLEDevice::init("Pulze Footswitch");
  bleTransfer.begin(); // starts advertising immediately, stays on
  bleAmp.begin();      // MTU preference only - does not connect yet

  bleRequestQueue = xQueueCreate(4, sizeof(BleRequest));

  xTaskCreatePinnedToCore(bleTaskFn, "bleTask", 8192, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(uiTaskFn, "uiTask", 8192, nullptr, 1, nullptr, 1);

  // Arduino's own loop() stays empty - everything runs in the two
  // tasks above.
}

void loop() {
  vTaskDelete(nullptr); // this task (Arduino's default loop task) isn't needed
}
