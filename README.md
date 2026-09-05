# Pulze // Control

A browser app that talks to a Hotone Pulze Mini over BLE-MIDI: recall
saved patches, capture new ones from the amp, toggle the tuner, and
optionally transfer a Favorites list to a companion ESP32 footswitch.

Firmware for that footswitch lives in
[`hw-footswitch-controller/`](hw-footswitch-controller/).

---

## User guide

### What you need

- A Pulze Mini (or compatible BLE-MIDI Pulze amp)
- **Chrome, Edge, or Brave on Android** (or Chrome on a desktop with
  Bluetooth). Firefox, Samsung Internet, and every iOS browser cannot
  use Web Bluetooth.
- The page must load as a **secure context**: `https://` or
  `http://localhost`. Opening the `.html` file directly (`file://`)
  often blocks Bluetooth.

### How to open the app

**Simplest:** open this link in Chrome (or Edge/Brave):

**https://mongpt.github.io/pulze-controller/**

You can add that page to the phone's home screen for a one-tap launch.

**Other options** if you want a local copy:

- **Host it yourself** - upload `index.html`, `style.css`, and `app.js`
  to any static host (GitHub Pages works).
- **Serve it from the phone, fully offline** - install
  [Termux](https://f-droid.org/packages/com.termux/) from F-Droid, then:

  ```
  pkg install python
  cd /path/to/these/files
  python -m http.server 8000
  ```

  Open Chrome at `http://localhost:8000`. Leave Termux running in the
  background while you use the page.
- **Open the file directly** - double-tap `index.html` in Chrome. If
  Connect does nothing, use the hosted link or Termux instead.

### Connect to the amp

There is no device list on the page. Tap **Connect to amp**. Chrome
opens its own Bluetooth picker - choose the Pulze. That picker is a
browser security rule: a web page cannot silently scan for nearby
devices.

The header LED turns green when connected. Tap **Disconnect amp** when
you are done.

### Recall a patch

Tap any tile in **Patches** or **Favorites**. The amp loads that
snapshot. The selected tile gets a green border (the same highlight
the Tuner uses when it is on).

**Previous Tone** / **Next Tone** step through the amp's own stored
presets (not your saved library). Anything you land on can be
captured and saved.

### Tuner

A purple **Tuner** tile always sits first in Favorites. It is not a
saved preset and is never sent to the footswitch.

1. Tap Tuner once - the amp tuner turns on. The tile uses the same
   green selected look as a patch.
2. Tap Tuner again - tuner off, and the last selected patch is
   recalled if you had one.
3. Tap any other patch while the tuner is on - tuner off, then that
   patch loads.

### Favorites and the library

- Tap the star on a patch to pin it in **Favorites**. Order is the
  order you starred them (that order is what the footswitch uses).
- Tap the star again to unpin. The trash icon in Patches deletes a
  patch permanently.
- **Reset to defaults** replaces the whole library with the built-in
  16 patches. Export first if you want to keep your work.

### Capture a new patch

1. Change a tone on the amp (knobs, app, footswitch, or Previous/Next
   Tone). Capture status becomes **snapshot ready to save**.
2. Type a **Patch name**.
3. **Save to** a new slot, or overwrite an existing name.
4. Tap **Save patch**.

Presets live only in this browser's local storage. Clearing site data,
switching browsers, or changing phones loses them unless you export.

**Export library** / **Import library** write and read a
`pulze_presets.json` file you can back up or move to another device.

### Footswitch (optional)

The footswitch is a **different** Bluetooth device from the amp. The
app can be connected to both independently, but the footswitch itself
will not connect to the amp while the app is still attached to it.

1. Star patches in the order you want on the switches: 1st favorite →
   switch 1 / page 1, 2nd → switch 2 / page 1, … 5th → switch 1 / page 2.
2. Tap **Connect to Footswitch** and pick **Pulze Footswitch**.
3. Tap **Transfer to Footswitch**. That replaces the whole library on
   the device (it does not merge).
4. Tap **Disconnect Footswitch** so the hardware can connect to the
   amp.

Everyday footswitch use (hold Page Up + Page Down to connect to the
amp, switches 1–4 to recall, and so on) is documented in
[`hw-footswitch-controller/README.md`](hw-footswitch-controller/README.md).

---

## Technical findings and implementation

This section is for anyone writing another app, a MIDI controller, or
firmware that should speak the same language as this project. The
Pulze Mini does not expose a public "load preset N" MIDI message.
Exact tone recall is a BLE-MIDI dump of a captured snapshot, not a
program-change number.

### BLE-MIDI connection to the amp

The amp is a standard BLE-MIDI peripheral. Official MIDI BLE UUIDs
(same on every BLE-MIDI device, not Pulze-specific):

| Role | UUID |
| --- | --- |
| MIDI service | `03b80e5a-ede8-4b33-a751-6ce34ec4c700` |
| MIDI data characteristic | `7772e5db-3868-4112-a1a9-f2669d106bf3` |

Connect, subscribe to notifications on the data characteristic, and
write with **GATT Write Command** (`writeValueWithoutResponse` in Web
Bluetooth / `response=False` in Python). Small CC messages and the
preset dump both go to this one characteristic.

The Pulze Mini uses a **random** BLE address (not public). Native
clients that reconnect by stored address must persist **address +
address type**. Guessing public will fail silently. The ESP32
firmware learns this on first scan and stores it in NVS.

### MIDI Control Change (channel 0)

BLE-MIDI packet used here:

```
80 80  B0  <cc>  <value>
```

`80 80` is the BLE-MIDI header/timestamp prefix; `B0` is Control
Change on channel 0.

| CC | Values | Meaning |
| --- | --- | --- |
| 26 | 100 | Previous Tone (relative step in the amp's own list) |
| 27 | 100 | Next Tone |
| 55 | 0–63 off, 64–127 on | Tuner. This app sends **0** / **100** |

There is no MIDI message that jumps to "preset index N" in the amp's
factory/user banks. Previous/Next only step. To land on a *specific*
saved tone you captured yourself, send the snapshot dump below.

### Capturing a tone snapshot

When the amp's current tone changes, it notifies two packets on the
MIDI characteristic:

| Notification length | Role |
| --- | --- |
| **194 bytes** | Full tone dump (`packet2`). This is what we store. |
| **88 bytes** | Follow-up (`packet3`). Used as a "capture complete" signal. |

The app keeps `packet2` when a 194-byte notification arrives, then
treats the next 88-byte notification as "snapshot ready." Saving a
patch stores **name + hex of those 194 bytes** only. `packet3` is
not persisted; a fixed template is sent on recall instead.

Stepping with CC 26/27 also fires these notifications, so you can
browse the amp and save anything you like.

### Recalling a stored patch

Three writes, in order, with pauses:

1. **packet2** (194 bytes) - the saved dump. Byte index **7** is a
   running MIDI sequence nibble: start at `0x2C`, increment, wrap
   back to `0x2C` after `0x7F`. Patch that byte before sending.
2. Wait **100 ms**.
3. **packet3** - the 88-byte template in `app.js`
   (`PACKET3_TEMPLATE_HEX`). Same sequence byte at index 7.
4. Wait **300 ms**.
5. **Commit** - fixed 16-byte packet
   `8080f0212541500000021404010401f7`.

That sequence is what actually applies the tone. Skipping the commit
or the delays is a common reason a dump "sends" but the amp does not
change.

### ATT MTU (why Play can fail when CC works)

packet2 is 194 bytes. It must go out as **one** write, which needs
ATT_MTU ≥ about 197. Native stacks can request this (`NimBLEDevice::setMTU(247)`
in the footswitch firmware). **Web Bluetooth has no JS API to request
or read MTU.** Android Chrome usually negotiates a large enough MTU on
its own. If Connect and Tuner/Previous/Next work but tapping a patch
fails, that is almost certainly MTU, not a bug in the hex dumps.

### Tuner behavior in this app

Tuner is UI-only state plus CC 55. It is not part of the preset JSON
and is not transferred to the footswitch.

- On: CC 55 value 100.
- Off: CC 55 value 0, then if a patch was already selected, send that
  patch's dump so the amp leaves tuner on the last tone rather than
  an empty bypass.

### Preset library format

`localStorage` key `pulze_presets_v1`:

```json
{
  "presets": [
    { "id": "uuid", "name": "Category|Title", "packet2": "<388 hex chars>", "favorite": true }
  ],
  "favoritesOrder": ["uuid", "..."]
}
```

`packet2` is 194 bytes encoded as 388 hex characters. `favoritesOrder`
is add-order, not alphabetical (the Patches list is sorted by name).
Older files that are a bare array of `{ name, packet2 }` still import.

### Custom BLE protocol: app → footswitch

This is **not** MIDI. The footswitch advertises its own service so the
browser can push a library without going through the amp.

| Role | UUID |
| --- | --- |
| Custom service | `e6f80001-b5f0-4eea-9a1e-31b0d6cfa930` |
| Transfer characteristic | `e6f80002-b5f0-4eea-9a1e-31b0d6cfa930` |

Writes use **Write With Response** (so order is acknowledged). Each
write starts with a type tag (v2 - a magic first data byte was
ambiguous because payload bytes are arbitrary):

| Tag | Message |
| --- | --- |
| `0x01` | START: `[0x01, countLo, countHi]` little-endian preset count |
| `0x02` | DATA: `[0x02, ...up to 17 payload bytes]` - concatenate in order |
| `0x03` | END: `[0x03]` - parse the buffer now |

Max write length is **18 bytes** including the tag (safe default ATT
payload). After END, the buffer is `count` records back-to-back:

```
[nameLenLo, nameLenHi, ...UTF-8 name..., ...194-byte packet2...]
```

Record 0 → footswitch 1 / page 0, record 1 → switch 2 / page 0, …
`page = floor(i / 4)`, `switch = i % 4`. Each transfer **replaces**
the stored library.

The footswitch is a BLE **peripheral** toward the app and a BLE
**central** toward the amp. This project does not use both roles at
once: disconnect the app before the hardware connects to the amp.

### Files to copy if you reimplement

| File | What it encodes |
| --- | --- |
| `app.js` | Web Bluetooth connect, CC helpers, dump/recall, capture, tuner, footswitch transfer |
| `hw-footswitch-controller/BleAmp.cpp` | Native BLE-MIDI client, MTU 247, address-type handling |
| `hw-footswitch-controller/BleTransfer.cpp` / `TransferParser.cpp` | Receiver for the tagged stream above |
