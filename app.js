// ---------------------------------------------------------------------
// Pulze // Control - standalone Android build (Chrome + Web Bluetooth)
//
// No backend at all - every BLE call in this file talks directly to
// the amp via navigator.bluetooth. This only works in Chromium-based
// browsers (Chrome, Edge, Brave - NOT Firefox, NOT Safari/iOS), and
// only in a "secure context" (https://, or http://localhost - a plain
// file:// page will likely be blocked from using Bluetooth at all).
//
// MIDI_SERVICE_UUID / DATA_CHAR_UUID are the official BLE-MIDI spec
// UUIDs, identical on every BLE-MIDI device regardless of manufacturer
// - same values used throughout the Python version.
//
// KNOWN RISK, untested against real hardware from here: the 178-byte
// preset-load write (packet2) needs the phone/Chrome to have
// negotiated a large enough ATT MTU. Android+Chrome usually does this
// automatically, but Web Bluetooth provides no JS API to request or
// verify the MTU explicitly (unlike native BLE code). If small
// commands (Connect, any future CC-style messages) work but "Play"
// fails, this is almost certainly why - it's a platform limitation,
// not a bug to fix in this file.
//
// FOOTSWITCH_SERVICE_UUID / FOOTSWITCH_CHAR_UUID are NOT official
// anything - custom, made-up UUIDs for a future ESP32 footswitch
// controller (separate device from the amp) to advertise. This is a
// completely different BLE connection/session, made and torn down
// independently of the amp connection - see transferFavoritesToFootswitch()
// and the wire protocol documented there.
// ---------------------------------------------------------------------

const MIDI_SERVICE_UUID = "03b80e5a-ede8-4b33-a751-6ce34ec4c700";
const DATA_CHAR_UUID = "7772e5db-3868-4112-a1a9-f2669d106bf3";

const FOOTSWITCH_SERVICE_UUID = "e6f80001-b5f0-4eea-9a1e-31b0d6cfa930";
const FOOTSWITCH_CHAR_UUID = "e6f80002-b5f0-4eea-9a1e-31b0d6cfa930";

const PACKET_COMMIT_HEX = "8080f0212541500000021404010401f7";
const PACKET3_TEMPLATE_HEX =
  "8f00000000000000000000000000050e000200000001000300040005000600" +
  "000000000000000005000000000007010b000000000000060a00000000000a" +
  "0305000000000001000c000000000004010200000000000cd6f7";

const STORAGE_KEY = "pulze_presets_v1";

// Seeds localStorage on first run only - your existing 16-patch
// library from the Python version, carried over automatically.
const DEFAULT_PRESETS = [{"name": "Bass|Fixup", "packet2": "9eb6f02125415000000212040100000006000000020000070c0000000000000003000b0002000000000000010404020000000008080402000000000f000401000000000800040200000000070004010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000050c020807080000040606090708070507000000000000000000000000000000000000"}, {"name": "Acoustic|Lullaby", "packet2": "bcfcf02125415000000212040100000006000000020000070c0000000000000004000a000200000000000004080402000000000a000401000000000c080402000000000c080402000000000408040200000000020004020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000704050507080000040c0705060c060c06010602070900000000000000000000000000"}, {"name": "Vintage|Flip Top", "packet2": "8fcef02125415000000212040100000006000000020000070c00000000000000050103000200000000000001080402000000000408040200000000040804020000000008060402000000000e00040100000000040c0402000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000050a020d070800000406060c0609070002000504060f07000000000000000000000000"}, {"name": "Modern|Area 51", "packet2": "a4d7f02125415000000212040100000006000000020000070c0000000000000006000f00020000000000000408040200000000040804020000000004080402000000000a0004010000000004080402000000000c0804020000000008040402000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000303040107080000040107020605060102000305030100000000000000000000000000"}, {"name": "Artist|TakDist", "packet2": "9aa6f02125415000000212040100000006000000020000070c0000000000000007010e0002000000000000080404020000000006000402000000000408040200000000050404020000000008000402000000000300040200000000000000000000000000000000000000000800030f00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004050401060d000005040601060b040406090703070400000000000000000000000000"}, {"name": "Pop/Rock|TakNegai", "packet2": "a3a0f02125415000000212040100000006000000020000070c0000000000000008010f0002000000000000030004010000000008080402000000000408040200000000080c040200000000090004020000000000080402000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000040404060708000005040601060b040e06050607060106090000000000000000000000"}, {"name": "Blues/Roots|Mcz Blues", "packet2": "93d3f02125415000000212040100000006000000020000070c0000000000000009000600020000000000000c0804010000000002000402000000000e0804010000000007080402000000000b0e04020000000000000000000000000800030f000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000101030206010000040d0603070a02000402060c070506050703000000000000000000"}, {"name": "Funk/Soul|Funky Phaser", "packet2": "a3d1f02125415000000212040100000006000000020000070c000000000000000a000100030000000000000504040200000000000804020000000001040402000000000f08040100000000050c04020000000000000000000000000800030f00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000906040708000004060705060e060b07090200050006080601070306050702000000"}, {"name": "Metal/Djent|Saga Metal", "packet2": "a8b9f02125415000000212040100000006000000020000070c000000000000000b0101000200000000000004080402000000000408040200000000040804020000000004080402000000000408040200000000040804020000000008020402000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000307040b0708000005030601060706010200040d060507040601060c00000000000000"}, {"name": "Jazz/Fusion|Tweed Jazz", "packet2": "808bf02125415000000212040100000006000000020000070c000000000000000c000a0002000000000000000c040200000000040804020000000004080402000000000408040200000000000000000000000004080402000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010d050a07080000050407070605060506040200040a0601070a070a00000000000000"}, {"name": "Alt./Special|Ice Peak", "packet2": "bcf7f02125415000000212040100000006000000020000070c000000000000000d010b000200000000000004080402000000000b00040100000000080c040200000000070004020000000008020402000000000408040200000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007020302070800000409060306050200050006050601060b0000000000000000000000"}, {"name": "All Tones|80s AOR", "packet2": "bbe3f02125415000000212040100000006000000020000070c0000000000000f0f00080000000000000000040804020000000002080402000000000a0004020000000004080402000000000000000000000000030004020000000004080402000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000008030c0708000003080300070302000401040f050200000000000000000000000000"}, {"name": "Clean|Mcz Clean", "packet2": "a6def02125415000000212040100000006000000020000070c0000000000000000000500020000000000000c080401000000000f00040100000000040804020000000002000402000000000c08040200000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000100030207080000040d0603070a02000403060c06050601060e000000000000000000"}, {"name": "Drive|Tight Drive", "packet2": "829ef02125415000000212040100000006000000020000070c0000000000000001000100020000000000000c000402000000000408040200000000030004020000000004080402000000000408040200000000080604020000000004080402000000000800030f000000000800030f000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000a020b07080000050406090607060807040200040407020609070606050000000000"}, {"name": "Hi Gain|Soloist Lead", "packet2": "af95f02125415000000212040100000006000000020000070c00000000000000020007000200000000000004080402000000000906040200000000040804020000000002000402000000000a00040100000000050c04020000000007000402000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000208040e070800000503060f060c060f0609070307040200040c060506010604000000"}];

// ---- DOM references ----------------------------------------------------

const led = document.getElementById("led");
const connText = document.getElementById("connText");
const connectBtn = document.getElementById("connectBtn");
const btHint = document.getElementById("btHint");
const prevBtn = document.getElementById("prevBtn");
const nextBtn = document.getElementById("nextBtn");

const patchList = document.getElementById("patchList");
const patchEmpty = document.getElementById("patchEmpty");
const favoriteList = document.getElementById("favoriteList");
const favoriteEmpty = document.getElementById("favoriteEmpty");

const captureStatus = document.getElementById("captureStatus");
const nameInput = document.getElementById("nameInput");
const slotSelect = document.getElementById("slotSelect");
const saveBtn = document.getElementById("saveBtn");

const exportBtn = document.getElementById("exportBtn");
const importInput = document.getElementById("importInput");
const resetBtn = document.getElementById("resetBtn");
const transferBtn = document.getElementById("transferBtn");
const connectFootswitchBtn = document.getElementById("connectFootswitchBtn");

const logEl = document.getElementById("log");

// ---- state ---------------------------------------------------------

let bleDevice = null;
let gattServer = null;
let characteristic = null;

// Footswitch controller state - a completely separate BLE connection
// from the amp's, connected/disconnected independently and explicitly
// via its own button, so the footswitch's own peripheral connection
// slot can be freed on demand (it needs to be free before the
// footswitch itself can connect out to the amp - see the firmware's
// gesture-driven design).
let fsDevice = null;
let fsCharacteristic = null;

let capturePacket2 = null; // Uint8Array
let capturePacket3 = null; // Uint8Array

let { presets, favoritesOrder } = loadState();
sortPresets();
let activeIndex = null;
let seq = 0x2c;

// ---- helpers -------------------------------------------------------

function hexToBytes(hex) {
  const bytes = new Uint8Array(hex.length / 2);
  for (let i = 0; i < bytes.length; i++) {
    bytes[i] = parseInt(hex.substr(i * 2, 2), 16);
  }
  return bytes;
}

function bytesToHex(bytes) {
  return Array.from(bytes).map(b => b.toString(16).padStart(2, "0")).join("");
}

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function log(msg) {
  logEl.textContent += msg + "\n";
  logEl.scrollTop = logEl.scrollHeight;
  console.log(msg);
}

// ---- localStorage persistence ----------------------------------------

function generateId() {
  if (typeof crypto !== "undefined" && crypto.randomUUID) {
    return crypto.randomUUID();
  }
  return "id-" + Date.now() + "-" + Math.random().toString(16).slice(2);
}

function ensureIds(list) {
  list.forEach(p => {
    if (!p.id) p.id = generateId();
  });
  return list;
}

// Normalizes any of the shapes this app has ever saved into
// { presets, favoritesOrder }:
//  - a bare array (original format, no ids, favorite was just a bool
//    with no memory of order) - order is reconstructed as best-effort
//    (whatever order they appear in), since true add-order was never
//    recorded in that format.
//  - { presets, favoritesOrder } (current format).
function normalizeState(parsed) {
  if (Array.isArray(parsed)) {
    const list = ensureIds(parsed);
    const order = list.filter(p => p.favorite).map(p => p.id);
    return { presets: list, favoritesOrder: order };
  }
  const list = ensureIds(parsed.presets || []);
  const validIds = new Set(list.map(p => p.id));
  const order = Array.isArray(parsed.favoritesOrder)
    ? parsed.favoritesOrder.filter(id => validIds.has(id))
    : list.filter(p => p.favorite).map(p => p.id);
  return { presets: list, favoritesOrder: order };
}

function loadState() {
  const raw = localStorage.getItem(STORAGE_KEY);
  if (raw) {
    try {
      return normalizeState(JSON.parse(raw));
    } catch (e) {
      console.error("Corrupt preset storage, reseeding defaults.", e);
    }
  }
  // Clone each entry, not just the array - .slice() alone would leave
  // every object shared with DEFAULT_PRESETS by reference, so favoriting
  // or overwriting a freshly-seeded preset would silently mutate the
  // constant itself.
  const seeded = ensureIds(DEFAULT_PRESETS.map(p => ({ ...p })));
  return { presets: seeded, favoritesOrder: [] };
}

function saveState() {
  localStorage.setItem(STORAGE_KEY, JSON.stringify({ presets, favoritesOrder }));
}

function sortPresets() {
  presets.sort((a, b) => a.name.localeCompare(b.name, undefined, { sensitivity: "base" }));
}

// ---- UI rendering ----------------------------------------------------

function setLed(status) {
  led.classList.remove("led--off", "led--connecting", "led--on");
  if (status === "connected") {
    led.classList.add("led--on");
    connText.textContent = "connected";
    connectBtn.textContent = "Disconnect amp";
  } else if (status === "connecting") {
    led.classList.add("led--connecting");
    connText.textContent = "connecting...";
    connectBtn.textContent = "Connect to amp";
  } else {
    led.classList.add("led--off");
    connText.textContent = "not connected";
    connectBtn.textContent = "Connect to amp";
  }
}

function createPatchTile(preset, index, { showDelete = false } = {}) {
  // A div, not a <button> - so the real <button> star/trash controls
  // can legally nest inside it (buttons can't contain buttons).
  // Manually wired for click + keyboard (Enter/Space) to keep it
  // accessible.
  const tile = document.createElement("div");
  tile.className = "patch-tile" + (index === activeIndex ? " patch-tile--active" : "");
  tile.dataset.index = index;
  tile.setAttribute("role", "button");
  tile.tabIndex = 0;

  const icons = document.createElement("div");
  icons.className = "patch-tile__icons";

  // Permanent delete only makes sense from the main Patches list - in
  // the Favorites panel, removal should just mean "unfavorite" (the
  // star already does that), not "destroy the preset". So this button
  // is only built when showDelete is true.
  if (showDelete) {
    const delBtn = document.createElement("button");
    delBtn.type = "button";
    delBtn.className = "patch-tile__del";
    delBtn.setAttribute("aria-label", "Delete permanently");
    delBtn.innerHTML =
      '<svg viewBox="0 0 16 16" width="14" height="14" fill="none" ' +
      'stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round">' +
      '<path d="M2.5 4.5h11"/>' +
      '<path d="M5.5 4.5V2.7a1 1 0 0 1 1-1h3a1 1 0 0 1 1 1v1.8"/>' +
      '<path d="M3.7 4.5l0.6 8.6a1 1 0 0 0 1 0.9h5.4a1 1 0 0 0 1-0.9l0.6-8.6"/>' +
      '<path d="M6.3 7.2v3.6"/>' +
      '<path d="M9.7 7.2v3.6"/>' +
      "</svg>";
    delBtn.addEventListener("click", (e) => {
      e.stopPropagation();
      deletePreset(index);
    });
    icons.appendChild(delBtn);
  }

  const favBtn = document.createElement("button");
  favBtn.type = "button";
  favBtn.className = "patch-tile__fav" + (preset.favorite ? " patch-tile__fav--on" : "");
  favBtn.setAttribute("aria-label", preset.favorite ? "Remove from favorites" : "Add to favorites");
  favBtn.textContent = preset.favorite ? "★" : "☆";
  favBtn.addEventListener("click", (e) => {
    e.stopPropagation();
    toggleFavorite(index);
  });

  icons.appendChild(favBtn);

  const nameSpan = document.createElement("span");
  nameSpan.className = "patch-tile__name";
  nameSpan.textContent = preset.name;

  tile.appendChild(icons);
  tile.appendChild(nameSpan);

  const trigger = () => {
    tile.classList.add("patch-tile--sending");
    playPreset(index).finally(() => tile.classList.remove("patch-tile--sending"));
  };
  tile.addEventListener("click", trigger);
  tile.addEventListener("keydown", (e) => {
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault();
      trigger();
    }
  });

  return tile;
}

function deletePreset(index) {
  const preset = presets[index];
  if (!preset) return;

  if (!confirm(`Delete "${preset.name}" permanently? This can't be undone.`)) {
    return;
  }

  presets.splice(index, 1);
  favoritesOrder = favoritesOrder.filter(id => id !== preset.id);

  // Every index after the deleted one shifts down by one - keep the
  // active-patch highlight pointing at the right entry, or clear it if
  // the deleted patch was the one that was active.
  if (activeIndex === index) {
    activeIndex = null;
  } else if (activeIndex !== null && activeIndex > index) {
    activeIndex -= 1;
  }

  saveState();
  renderPatches();
  log(`Deleted: ${preset.name}`);
}

function toggleFavorite(index) {
  const preset = presets[index];
  if (!preset) return;

  preset.favorite = !preset.favorite;

  if (preset.favorite) {
    // Newly favorited - goes to the END of the add-order list, so the
    // Favorites panel (and later, the footswitch transfer) reflects the
    // order things were actually favorited in, not alphabetical order.
    if (!favoritesOrder.includes(preset.id)) {
      favoritesOrder.push(preset.id);
    }
  } else {
    favoritesOrder = favoritesOrder.filter(id => id !== preset.id);
  }

  saveState();
  renderPatches();
}

function renderPatches() {
  patchList.innerHTML = "";
  if (presets.length === 0) {
    patchList.appendChild(patchEmpty);
  } else {
    presets.forEach((p, i) => patchList.appendChild(createPatchTile(p, i, { showDelete: true })));
  }

  favoriteList.innerHTML = "";
  const favIndices = favoritesOrder
    .map(id => presets.findIndex(p => p.id === id))
    .filter(i => i !== -1);
  if (favIndices.length === 0) {
    favoriteList.appendChild(favoriteEmpty);
  } else {
    favIndices.forEach(i => favoriteList.appendChild(createPatchTile(presets[i], i)));
  }

  const prevValue = slotSelect.value;
  slotSelect.innerHTML = `<option value="">New slot (${presets.length})</option>`;
  presets.forEach((p, i) => {
    const opt = document.createElement("option");
    opt.value = i;
    opt.textContent = `${p.name} (overwrite)`;
    slotSelect.appendChild(opt);
  });
  if ([...slotSelect.options].some(o => o.value === prevValue)) {
    slotSelect.value = prevValue;
  }
}

function renderCaptureStatus(ready) {
  captureStatus.innerHTML = ready
    ? `<span class="dot dot--ready"></span><span>snapshot ready to save</span>`
    : `<span class="dot dot--off"></span><span>waiting for a tone change on the amp</span>`;
}

// ---- Bluetooth: connect ----------------------------------------------

async function connect() {
  if (!navigator.bluetooth) {
    log("This browser doesn't support Web Bluetooth. Use Chrome on Android.");
    return;
  }

  try {
    setLed("connecting");
    log("Requesting device (pick your Pulze in the browser's own dialog)...");

    bleDevice = await navigator.bluetooth.requestDevice({
      filters: [{ services: [MIDI_SERVICE_UUID] }],
    });

    bleDevice.addEventListener("gattserverdisconnected", onDisconnected);

    log(`Connecting to ${bleDevice.name || bleDevice.id}...`);
    gattServer = await bleDevice.gatt.connect();

    const service = await gattServer.getPrimaryService(MIDI_SERVICE_UUID);
    characteristic = await service.getCharacteristic(DATA_CHAR_UUID);

    await characteristic.startNotifications();
    characteristic.addEventListener("characteristicvaluechanged", onNotification);

    setLed("connected");
    log(`Connected to ${bleDevice.name || bleDevice.id}.`);
  } catch (e) {
    setLed("disconnected");
    log("Connect failed: " + e.message);
  }
}

function onDisconnected() {
  setLed("disconnected");
  characteristic = null;
  activeIndex = null;
  renderPatches();
  log("Disconnected.");
}

function disconnectAmp() {
  if (bleDevice && bleDevice.gatt.connected) {
    log("Disconnecting...");
    bleDevice.gatt.disconnect(); // fires the gattserverdisconnected event -> onDisconnected()
  }
}

// ---- Bluetooth: notifications (tone snapshot capture) ------------------

function onNotification(event) {
  const data = new Uint8Array(event.target.value.buffer);
  if (data.length === 194) {
    capturePacket2 = data;
  } else if (data.length === 88) {
    capturePacket3 = data;
    if (capturePacket2) {
      log("New tone snapshot captured.");
      renderCaptureStatus(true);
    }
  }
}

// ---- Bluetooth: sending a preset ---------------------------------------

async function writeBytes(bytes) {
  // writeValueWithoutResponse matches the GATT "Write Command" used
  // throughout the Python version (response=False) - same wire format.
  await characteristic.writeValueWithoutResponse(bytes);
}

async function sendPreset(entry) {
  const p2 = hexToBytes(entry.packet2);
  const p3 = hexToBytes(PACKET3_TEMPLATE_HEX);

  p2[7] = seq;
  p3[7] = seq;
  seq = seq >= 0x7f ? 0x2c : seq + 1;

  await writeBytes(p2);
  await sleep(100);
  await writeBytes(p3);
  await sleep(300);
  await writeBytes(hexToBytes(PACKET_COMMIT_HEX));

  log(`Sent: ${entry.name}`);
}

async function playPreset(index) {
  if (!characteristic) {
    alert("Not connected to an amp.");
    return;
  }
  const entry = presets[index];
  if (!entry) return;

  try {
    await sendPreset(entry);
    activeIndex = index;
    renderPatches();
  } catch (e) {
    log("Play failed: " + e.message);
  }
}

// ---- explore: previous/next tone (Hotone's own MIDI Control Change
// interface for stepping through the amp's stored presets, same as
// the CLI script's 'next'/'prev' commands - CC27 = Next Tone,
// CC26 = Previous Tone, both on channel 0. This is a relative step,
// not "jump to preset N" - the amp has no MIDI message for that. Since
// notifications aren't mode-gated, stepping through tones this way
// will also populate the capture/save panel just like any other tone
// change, so you can save anything interesting you land on. ----------

async function sendControlChange(channel, ccNumber, value) {
  const packet = new Uint8Array([0x80, 0x80, 0xb0 | channel, ccNumber, value]);
  await writeBytes(packet);
}

async function stepTone(direction) {
  if (!characteristic) {
    alert("Not connected to an amp.");
    return;
  }
  const ccNumber = direction === "next" ? 27 : 26;
  try {
    await sendControlChange(0, ccNumber, 100);
    log(direction === "next" ? "Next tone." : "Previous tone.");
  } catch (e) {
    log("Step failed: " + e.message);
  }
}

// ---- saving a captured snapshot -----------------------------------------

function savePreset(name, slot) {
  name = name.trim();
  if (!name) {
    alert("Enter a patch name first.");
    return;
  }
  if (!capturePacket2 || !capturePacket3) {
    alert("No snapshot captured yet - change a tone on the amp first.");
    return;
  }

  const entry = { id: generateId(), name, packet2: bytesToHex(capturePacket2) };

  let logMsg;
  if (slot !== null && slot >= 0 && slot < presets.length) {
    entry.id = presets[slot].id; // keep identity so favoritesOrder still finds it after a rename
    entry.favorite = presets[slot].favorite; // carry favorite status over when overwriting
    presets[slot] = entry;
    logMsg = `Overwrote: ${name}`;
  } else {
    presets.push(entry);
    logMsg = `Saved: ${name}`;
  }

  sortPresets();
  saveState();
  // Find where the saved entry actually landed after sorting - its
  // position may have moved, but the object itself is the same
  // reference, so identity lookup finds it reliably regardless of
  // where alphabetical order put it.
  activeIndex = presets.indexOf(entry);
  renderPatches();
  log(logMsg);
}

// ---- transfer to footswitch controller ----------------------------------
//
// This talks to a COMPLETELY SEPARATE BLE device from the amp - a future
// ESP32 footswitch controller, acting as its own BLE peripheral with its
// own custom service (FOOTSWITCH_SERVICE_UUID). It is not connected to
// the amp at the same time as this transfer happens; the workflow is:
// build/curate the Favorites list here, transfer it to the footswitch,
// disconnect, then the footswitch separately connects to the amp on its
// own (as a BLE central) to actually recall presets live.
//
// WIRE PROTOCOL (v2) - this is the spec the ESP32 firmware implements
// on the receiving end (see BleTransfer.cpp / TransferParser.cpp).
//
// Everything is written to FOOTSWITCH_CHAR_UUID using writeValueWithResponse
// (not writeValueWithoutResponse) so each write is acknowledged before the
// next is sent - this gives reliable, in-order delivery for free from the
// BLE stack itself, without needing our own sequence numbers or checksums.
//
// Every write starts with an explicit 1-byte message-type tag as its
// very first byte - this is what makes control messages unambiguous
// from data, since data payload bytes are arbitrary preset content
// with no restriction on what values they contain. (v1 of this
// protocol tried to tell messages apart using a "magic" first-byte
// value instead of an explicit tag, and had a real, if rare, bug: a
// data chunk could coincidentally start with that same value and get
// misread as a control message. v2 fixes this properly.)
//
//   START: [0x01, countLo, countHi]  - countLo/Hi = little-endian
//          uint16 total preset count about to be sent. (3 bytes)
//   DATA:  [0x02, ...up to 17 payload bytes]  - firmware strips the tag
//          and appends the remaining bytes, in order, to one growing
//          receive buffer. Chunk boundaries carry no meaning of their
//          own; only the accumulated byte order matters. (<=18 bytes)
//   END:   [0x03]  - signals "the full stream has been sent, parse it
//          now." (1 byte)
//
// Once END arrives, the firmware parses the accumulated buffer as exactly
// `count` back-to-back records, each shaped:
//   [nameLenLo, nameLenHi, ...nameBytes (UTF-8, nameLenLo/Hi bytes long),
//    ...packet2 (always exactly 194 bytes)]
//
// Presets arrive in Favorites add-order (favoritesOrder), which is exactly
// the order they should be assigned to footswitches/banks: record 0 ->
// fw1/bank0, record 1 -> fw2/bank0, record 2 -> fw3/bank0, record 3 ->
// fw4/bank0, record 4 -> fw1/bank1, and so on (bank = floor(i / 4),
// fw = i % 4) - the firmware does this math, nothing bank-specific is
// sent over the wire.

const FS_MSG_START = 0x01;
const FS_MSG_DATA = 0x02;
const FS_MSG_END = 0x03;
const FOOTSWITCH_MAX_WRITE_LEN = 18; // total bytes per write, tag included

function buildFootswitchPayload(favoritePresets) {
  const encoder = new TextEncoder();
  const records = favoritePresets.map(p => {
    const nameBytes = encoder.encode(p.name);
    const record = new Uint8Array(2 + nameBytes.length + 194);
    record[0] = nameBytes.length & 0xff;
    record[1] = (nameBytes.length >> 8) & 0xff;
    record.set(nameBytes, 2);
    record.set(hexToBytes(p.packet2), 2 + nameBytes.length);
    return record;
  });

  const totalLen = records.reduce((sum, r) => sum + r.length, 0);
  const stream = new Uint8Array(totalLen);
  let offset = 0;
  for (const r of records) {
    stream.set(r, offset);
    offset += r.length;
  }
  return stream;
}

// ---- footswitch connection lifecycle (separate from transferring) ------
//
// Connecting and transferring are deliberately split into two actions,
// same pattern as the amp: "Connect to Footswitch" establishes a
// persistent connection you control explicitly, and "Transfer to
// Footswitch" just uses whatever connection is already there. This
// also means the connection can be torn down on demand (rather than
// only ever automatically after a transfer) - the footswitch's own
// firmware needs its peripheral connection slot free before IT can
// connect out to the amp, so having an explicit, reliable "disconnect
// now" action matters here more than it does for the amp.

function onFootswitchDisconnected() {
  fsDevice = null;
  fsCharacteristic = null;
  connectFootswitchBtn.textContent = "Connect to Footswitch";
  log("Footswitch disconnected.");
}

async function connectFootswitch() {
  if (!navigator.bluetooth) {
    log("This browser doesn't support Web Bluetooth. Use Chrome on Android.");
    return;
  }

  try {
    log("Requesting footswitch controller (pick it in the browser's own dialog)...");
    fsDevice = await navigator.bluetooth.requestDevice({
      filters: [{ services: [FOOTSWITCH_SERVICE_UUID] }],
    });
    fsDevice.addEventListener("gattserverdisconnected", onFootswitchDisconnected);

    // Deliberately not using fsDevice.name here - it can stay empty in
    // JS even when the chooser dialog correctly showed the real name
    // (a known Web Bluetooth inconsistency: the dialog reads from the
    // OS-level scan, device.name is populated separately and not
    // reliably across platforms). Since we filtered specifically by
    // this device's own dedicated service UUID, there's no ambiguity
    // about what it is anyway.
    log("Connecting to footswitch...");
    const fsServer = await fsDevice.gatt.connect();
    const service = await fsServer.getPrimaryService(FOOTSWITCH_SERVICE_UUID);
    fsCharacteristic = await service.getCharacteristic(FOOTSWITCH_CHAR_UUID);

    connectFootswitchBtn.textContent = "Disconnect Footswitch";
    log("Connected to footswitch.");
  } catch (e) {
    log("Footswitch connect failed: " + e.message);
    fsDevice = null;
    fsCharacteristic = null;
  }
}

function disconnectFootswitch() {
  if (fsDevice && fsDevice.gatt.connected) {
    fsDevice.gatt.disconnect(); // fires gattserverdisconnected -> onFootswitchDisconnected()
  }
}

async function transferFavoritesToFootswitch() {
  if (!fsCharacteristic) {
    alert("Not connected to the footswitch - tap \"Connect to Footswitch\" first.");
    return;
  }

  const favoritePresets = favoritesOrder
    .map(id => presets.find(p => p.id === id))
    .filter(Boolean);

  if (favoritePresets.length === 0) {
    alert("No favorites to transfer - add some from the Patches list first.");
    return;
  }

  try {
    const stream = buildFootswitchPayload(favoritePresets);

    const startMsg = new Uint8Array([
      FS_MSG_START,
      favoritePresets.length & 0xff,
      (favoritePresets.length >> 8) & 0xff,
    ]);
    await fsCharacteristic.writeValueWithResponse(startMsg);

    log(`Sending ${favoritePresets.length} preset(s) to footswitch...`);
    const payloadPerChunk = FOOTSWITCH_MAX_WRITE_LEN - 1; // -1 for the tag byte
    for (let i = 0; i < stream.length; i += payloadPerChunk) {
      const payload = stream.slice(i, i + payloadPerChunk);
      const chunk = new Uint8Array(payload.length + 1);
      chunk[0] = FS_MSG_DATA;
      chunk.set(payload, 1);
      await fsCharacteristic.writeValueWithResponse(chunk);
    }

    await fsCharacteristic.writeValueWithResponse(new Uint8Array([FS_MSG_END]));
    log("Transfer complete. Disconnect the footswitch when you're ready to use it with the amp.");
  } catch (e) {
    log("Footswitch transfer failed: " + e.message);
  }
}

// ---- export / import ----------------------------------------------------

function exportPresets() {
  const blob = new Blob(
    [JSON.stringify({ presets, favoritesOrder }, null, 2)],
    { type: "application/json" }
  );
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = "pulze_presets.json";
  a.click();
  URL.revokeObjectURL(url);
  log("Exported preset library.");
}

function importPresets(file) {
  const reader = new FileReader();
  reader.onload = () => {
    try {
      const parsed = JSON.parse(reader.result);
      if (!Array.isArray(parsed) && typeof parsed !== "object") {
        throw new Error("not a preset library");
      }
      const state = normalizeState(parsed);
      presets = state.presets;
      favoritesOrder = state.favoritesOrder;
      sortPresets();
      saveState();
      activeIndex = null;
      renderPatches();
      log(`Imported ${presets.length} preset(s), ${favoritesOrder.length} favorite(s).`);
    } catch (e) {
      alert("That doesn't look like a valid preset library file.");
    }
  };
  reader.readAsText(file);
}

function resetToDefaults() {
  if (!confirm(
    "Reset to the default preset list? This permanently deletes " +
    "all your saved patches and favorites. This can't be undone - " +
    "export your library first if you want to keep it."
  )) {
    return;
  }

  // Clone each entry rather than reusing the DEFAULT_PRESETS objects
  // directly - presets get mutated in place (favorite toggling,
  // overwriting), and DEFAULT_PRESETS needs to stay pristine so a
  // second reset later still produces the true original defaults.
  presets = ensureIds(DEFAULT_PRESETS.map(p => ({ ...p })));
  favoritesOrder = [];
  sortPresets();
  saveState();
  activeIndex = null;
  renderPatches();
  log("Reset to default preset list.");
}

// ---- wire up events ----------------------------------------------------

connectBtn.addEventListener("click", () => {
  if (bleDevice && bleDevice.gatt.connected) {
    disconnectAmp();
  } else {
    connect();
  }
});

prevBtn.addEventListener("click", () => stepTone("prev"));
nextBtn.addEventListener("click", () => stepTone("next"));

saveBtn.addEventListener("click", () => {
  const slotValue = slotSelect.value;
  const slot = slotValue === "" ? null : parseInt(slotValue, 10);
  savePreset(nameInput.value, slot);
  nameInput.value = "";
});

exportBtn.addEventListener("click", exportPresets);
resetBtn.addEventListener("click", resetToDefaults);
transferBtn.addEventListener("click", transferFavoritesToFootswitch);

connectFootswitchBtn.addEventListener("click", () => {
  if (fsDevice && fsDevice.gatt.connected) {
    disconnectFootswitch();
  } else {
    connectFootswitch();
  }
});

importInput.addEventListener("change", () => {
  if (importInput.files.length > 0) {
    importPresets(importInput.files[0]);
    importInput.value = "";
  }
});

// ---- init -------------------------------------------------------------

if (!navigator.bluetooth) {
  btHint.textContent = "Web Bluetooth not available - use Chrome on Android.";
}

renderPatches();
renderCaptureStatus(false);
setLed("disconnected");
