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
// ---------------------------------------------------------------------

const MIDI_SERVICE_UUID = "03b80e5a-ede8-4b33-a751-6ce34ec4c700";
const DATA_CHAR_UUID = "7772e5db-3868-4112-a1a9-f2669d106bf3";

const PACKET_COMMIT_HEX = "8080f0212541500000021404010401f7";
const PACKET3_TEMPLATE_HEX =
  "8f00000000000000000000000000050e000200000001000300040005000600" +
  "000000000000000005000000000007010b000000000000060a00000000000a" +
  "0305000000000001000c000000000004010200000000000cd6f7";

const STORAGE_KEY = "pulze_presets_v1";

// Seeds localStorage on first run only - your existing 16-patch
// library from the Python version, carried over automatically.
const DEFAULT_PRESETS = [{"name": "Tight Djent", "packet2": "bad7f02125415000000212040100000006000000020000070c000000000000000200040002000000000000020004020000000004080402000000000f000401000000000f0004010000000007000402000000000c080402000000000700040200000000000000000000000000000000000000000800030f000000000000000000000000000000000000000000000000000000000000000000000000000000000105030c070800000504060906070608070402000404060a0605060e07040000000000"}, {"name": "Classic 800", "packet2": "99e2f02125415000000212040100000006000000020000070c000000000000000200050002000000000000040804020000000004080402000000000408040200000000040804020000000004080402000000000408040200000000070004020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002010401070800000403060c0601070307030609060302000308030003000000000000"}, {"name": "Soloist Crunch", "packet2": "80ebf02125415000000212040100000006000000020000070c0000000000000002000600020000000000000a0004020000000004080402000000000408040200000000040804020000000004080402000000000408040200000000070004020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002070403070800000503060f060c060f0609070307040200040307020705060e060306"}, {"name": "Soloist Lead", "packet2": "b287f02125415000000212040100000006000000020000070c00000000000000020007000200000000000004080402000000000906040200000000040804020000000002000402000000000a00040100000000050c04020000000007000402000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000208040e070800000503060f060c060f0609070307040200040c060506010604000000"}, {"name": "Mcz 800", "packet2": "af97f02125415000000212040100000006000000020000070c0000000000000002000800020000000000000c08040100000000040804020000000004080402000000000c08040200000000040804020000000004080402000000000408040200000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000020d030d070b0000040d0603070a020003080300030000000000000000000000000000"}, {"name": "DP Lead One", "packet2": "99fbf02125415000000212040100000006000000020000070c0000000000000002000900020000000000000104040200000000010804020000000001040402000000000a000402000000000408040200000000040804020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000302020107080000040405000200040c0605060106040200040f060e06050000000000"}, {"name": "Toolkit", "packet2": "b1ddf02125415000000212040100000006000000020000070c0000000000000002010d000200000000000004080402000000000408040200000000040804020000000002040402000000000c00040100000000060c040200000000040804020000000000000000000000000800030f00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005000604070800000504060f060f060c060b0609070400000000000000000000000000"}, {"name": "Modern Rector", "packet2": "b4a3f02125415000000212040100000006000000020000070c0000000000000002000b000200000000000004080402000000000f00040100000000040804020000000004080402000000000d00040100000000040804020000000007000402000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000304040b07080000040d060f060406050702060e02000502060506030704060f070200"}, {"name": "MK4 Riff", "packet2": "84ddf02125415000000212040100000006000000020000070c0000000000000002000c0002000000000000080004020000000004080402000000000408040200000000040804020000000004080402000000000408040200000000070004020000000000000000000000000800030f0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000305050a07080000040d040b0304020005020609060606060000000000000000000000"}, {"name": "Rough Dizzle", "packet2": "bee5f02125415000000212040100000006000000020000070c0000000000000002000d00020000000000000408040200000000050c040200000000070004020000000004080402000000000c08040100000000040804020000000004080402000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000306040a070800000502060f070506070608020004040609070a070a060c0605000000"}, {"name": "Saga Metal", "packet2": "b4d5f02125415000000212040100000006000000020000070c0000000000000002000e000200000000000004080402000000000408040200000000040804020000000004080402000000000408040200000000040804020000000008020402000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000307040b0708000005030601060706010200040d060507040601060c00000000000000"}, {"name": "Chris Riff", "packet2": "aafcf02125415000000212040100000006000000020000070c0000000000000002000f0002000000000000040804020000000005040402000000000408040200000000030c0402000000000508040200000000060c0402000000000408040200000000000000000000000000000000000000000800030f000000000000000000000000000000000000000000000000000000000000000000000000000000000308030c07080000040306080702060907030200050206090606060600000000000000"}, {"name": "Cho's ThrashDist", "packet2": "86a4f02125415000000212040100000006000000020000070c0000000000000002010000020000000000000a00040200000000040804020000000004080402000000000500040200000000020c04020000000004040402000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000030a05050708000004030608060f020707030200050406080702060107030608040406"}, {"name": "Soloist Lead 1", "packet2": "aadef02125415000000212040100000006000000020000070c0000000000000002020000020000000000000a0a040200000000090604020000000004080402000000000408040200000000040804020000000008060402000000000800030f000000000800030f000000000800030f000000000800030f00000000000000000000000000000000000000000000000000000000000000000000000000000000070b040e070800000503060f060c060f0609070307040200040c060506010604020003"}, {"name": "Cocked Wah", "packet2": "bee5f02125415000000212040100000006000000020000070c0000000000000002010f0003000000000000020404020000000000000000000000000408040200000000080a040200000000040804020000000001080402000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000060c0501070800000403060f0603060b06050604020005070601060800000000000000"}, {"name": "DP Clean Verb", "packet2": "8fbff02125415000000212040100000006000000020000070c0000000000000000000400020000000000000f0004010000000004080402000000000408040200000000040804020000000007000402000000000800030f000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000c0406070800000404050002000403060c06050601060e0200050606050702060200"}];

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

const logEl = document.getElementById("log");

// ---- state ---------------------------------------------------------

let bleDevice = null;
let gattServer = null;
let characteristic = null;

let capturePacket2 = null; // Uint8Array
let capturePacket3 = null; // Uint8Array

let presets = loadPresets();
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

function loadPresets() {
  const raw = localStorage.getItem(STORAGE_KEY);
  if (raw) {
    try {
      return JSON.parse(raw);
    } catch (e) {
      console.error("Corrupt preset storage, reseeding defaults.", e);
    }
  }
  localStorage.setItem(STORAGE_KEY, JSON.stringify(DEFAULT_PRESETS));
  return DEFAULT_PRESETS.slice();
}

function savePresetsToStorage() {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(presets));
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

function createPatchTile(preset, index) {
  // A div, not a <button> - so the real <button> star toggle can
  // legally nest inside it (buttons can't contain buttons). Manually
  // wired for click + keyboard (Enter/Space) to keep it accessible.
  const tile = document.createElement("div");
  tile.className = "patch-tile" + (index === activeIndex ? " patch-tile--active" : "");
  tile.dataset.index = index;
  tile.setAttribute("role", "button");
  tile.tabIndex = 0;

  const favBtn = document.createElement("button");
  favBtn.type = "button";
  favBtn.className = "patch-tile__fav" + (preset.favorite ? " patch-tile__fav--on" : "");
  favBtn.setAttribute("aria-label", preset.favorite ? "Remove from favorites" : "Add to favorites");
  favBtn.textContent = preset.favorite ? "★" : "☆";
  favBtn.addEventListener("click", (e) => {
    e.stopPropagation();
    toggleFavorite(index);
  });

  const indexSpan = document.createElement("span");
  indexSpan.className = "patch-tile__index";
  indexSpan.textContent = index;

  const nameSpan = document.createElement("span");
  nameSpan.className = "patch-tile__name";
  nameSpan.textContent = preset.name;

  tile.appendChild(favBtn);
  tile.appendChild(indexSpan);
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

function toggleFavorite(index) {
  if (!presets[index]) return;
  presets[index].favorite = !presets[index].favorite;
  savePresetsToStorage();
  renderPatches();
}

function renderPatches() {
  patchList.innerHTML = "";
  if (presets.length === 0) {
    patchList.appendChild(patchEmpty);
  } else {
    presets.forEach((p, i) => patchList.appendChild(createPatchTile(p, i)));
  }

  favoriteList.innerHTML = "";
  const favIndices = presets.map((_, i) => i).filter(i => presets[i].favorite);
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
    opt.textContent = `${i}: ${p.name} (overwrite)`;
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

  const entry = { name, packet2: bytesToHex(capturePacket2) };

  let idx;
  if (slot !== null && slot >= 0 && slot < presets.length) {
    presets[slot] = entry;
    idx = slot;
    log(`Overwrote slot ${idx}: ${name}`);
  } else {
    presets.push(entry);
    idx = presets.length - 1;
    log(`Saved as new slot ${idx}: ${name}`);
  }

  savePresetsToStorage();
  activeIndex = idx;
  renderPatches();
}

// ---- export / import ----------------------------------------------------

function exportPresets() {
  const blob = new Blob([JSON.stringify(presets, null, 2)], { type: "application/json" });
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
      const data = JSON.parse(reader.result);
      if (!Array.isArray(data)) throw new Error("not a preset list");
      presets = data;
      savePresetsToStorage();
      activeIndex = null;
      renderPatches();
      log(`Imported ${data.length} preset(s).`);
    } catch (e) {
      alert("That doesn't look like a valid preset library file.");
    }
  };
  reader.readAsText(file);
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
