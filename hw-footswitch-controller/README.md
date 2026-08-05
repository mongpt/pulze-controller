# Pulze Footswitch Controller - Firmware

ESP32-S3-N16R8 firmware: 6 footswitches (4 presets + Page Up/Down) and
a 128x64 I2C SSD1306 OLED, 4 status LEDs. Connects directly to the
Pulze Mini over BLE (no phone/PC bridge needed) to recall exact saved
patches, and separately receives a curated preset library from the
companion web app over its own BLE connection.

**Confirmed working on a real ESP32-S3-N16R8 board against a real
Pulze Mini amp** - connecting, recalling presets, receiving a transfer
from the web app, reconnecting after a drop, and the OLED/LED feedback
have all been tested and verified on actual hardware, not just
reviewed as code.

## Required libraries (Arduino Library Manager)

- **NimBLE-Arduino** (h2zero) - written against the 2.x API surface
- **Adafruit_SSD1306**
- **Adafruit_GFX**

Board: **ESP32S3 Dev Module** (esp32 boards package by Espressif)

## Required settings (Arduino IDE, Tools menu)

- **Partition Scheme: Custom** - place the included `partitions.csv`
  in the same folder as `PulzeFootswitch.ino`. This defines a data
  partition explicitly named `"spiffs"` (not a typo - that's the exact
  label `LittleFS.begin(true)` looks for by default; the label doesn't
  need to match the filesystem type, just what the code asks for).
  Confirmed on real hardware: several of the board package's built-in
  16MB-flash presets name their data partition something else (e.g.
  `"ffat"`), which causes `LittleFS mount failed` at boot - this
  custom table avoids that entirely.
- **PSRAM: OPI PSRAM** - this is the N16R8 variant specifically (octal
  SPI PSRAM); the wrong PSRAM mode causes memory allocations to
  silently misbehave.
- **Upload Speed**: `921600` (drop to `115200` only if you get upload
  errors)
- **USB CDC On Boot**: `Disabled` for flashing/debugging over the UART
  port; flip to `Enabled` later if you want native-USB serial
  monitor/upload convenience instead.

## Wiring

See `config.h` for the full pin map and reasoning (avoids strapping
pins, the N16R8's octal-PSRAM-reserved pins, native USB D-/D+, and the
onboard RGB LED most S3 DevKitC boards wire to GPIO48). Verify against
your specific board's actual silkscreen before wiring, since GPIO
breakout availability varies between DevKitC revisions.

Footswitches: normally-open, one leg to the GPIO, other leg to GND.
Firmware uses `INPUT_PULLUP`, so no external resistor needed.

LEDs: GPIO -> ~220-330 ohm resistor -> LED anode -> LED cathode -> GND.

OLED: standard I2C SSD1306 module, SDA/SCL to the pins in `config.h`,
VCC/GND to 3.3V/GND. If it doesn't init, the two most common causes are
wrong I2C address (0x3C vs 0x3D - try both in `Display.cpp`) or SDA/SCL
swapped.

## Storage

- **LittleFS** (`/presets.bin`): the preset library itself. Only
  written once per transfer, not per button press - flash wear isn't
  a concern at that write frequency.
- **Preferences (NVS)**: the amp's remembered BLE address *and its
  address type* (public vs random - confirmed on real hardware that
  the Pulze Mini uses a random address; the firmware learns and
  persists this automatically the first time it scans, and defaults to
  guessing "random" first on a brand-new device), plus the last-active
  preset index. The index write is deliberately debounced (see
  `LAST_USED_SETTLE_MS`) - only committed to flash after 5 seconds
  with no further footswitch change, so rapid switching doesn't wear
  flash or leave "last used" pointing at something only briefly passed
  through. Playing a preset is never delayed by this - every press
  sends to the amp instantly regardless.

---

## How to use it

### The two BLE roles

This device talks to two different things over BLE, never at the same
time:

- **To the amp** - as a BLE *central* (the same role every other tool
  in this project has played), to actually recall presets.
- **To the web app** - as a BLE *peripheral*, to receive a curated
  preset library. This one runs continuously from the moment the
  device powers on - no special mode needed to make it available.

Because both roles share the same radio, the amp connection is
gesture-driven and deliberately refuses to start while the app is
still attached - you disconnect the app explicitly first. This is a
conservative design choice (true simultaneous dual-role operation may
work fine on this chip, but wasn't the design goal here), not a
platform limitation.

### First-time setup: building your preset library

1. Power on the footswitch. The OLED shows the normal performance
   screen; the device is already advertising as **"Pulze Footswitch"**
   in the background, ready for the app.
2. On the web app (phone or PC), tap **Connect to Footswitch** (in the
   Favorites panel) and pick "Pulze Footswitch" from the browser's
   device dialog.
3. Curate your Favorites list on the web app in the order you want
   presets assigned - **1st favorite -> footswitch 1 / page 1**, 2nd
   -> footswitch 2 / page 1, 3rd -> footswitch 3 / page 1, 4th ->
   footswitch 4 / page 1, 5th -> footswitch 1 / page 2, and so on
   (`page = index / 4`, `footswitch = index % 4`).
4. Tap **Transfer to Footswitch**. The OLED switches to showing
   "Receiving data from app" for the duration of the transfer, then
   briefly shows how many presets were saved, then returns to the
   normal performance screen.
5. Tap **Disconnect Footswitch** in the app once you're done - this
   frees up the device to connect to the amp next.

You can redo this any time to replace the whole library - each
transfer completely replaces what was there before, it doesn't merge.

### Everyday use: connecting to the amp and playing presets

1. **Hold footswitches 5 and 6 (Page Up + Page Down) together for 2
   seconds.** The OLED shows `AMP: connecting`, then either connects
   or falls back to a brief scan if it's the first time (or the amp
   moved/changed address). Once connected, the display shows `AMP OK`
   and **automatically re-sends whatever preset was last active**
   before you touch anything else - the amp ends up in the expected
   state right away, not wherever it happened to be left.
   - If you try this gesture while the web app is still connected, the
     footswitch refuses and shows a brief reminder to disconnect the
     app first.
2. **Footswitches 1-4** recall the preset in that slot on the
   currently-viewed page - instantly, and the corresponding LED lights
   up (the previous one turns off).
3. **Footswitches 5 and 6** (tapped individually, not held) move
   between pages without changing anything on the amp - just browsing.
4. **Hold footswitches 5 and 6 together again** to disconnect from the
   amp when you're done (e.g., to go build/update the library from the
   app again).

### Reading the OLED

- **Top row**: amp connection status (`AMP OK` / `AMP: connecting` /
  `AMP: scanning` / `AMP: --`).
- **Large inverted badge**: `PAGE <n>` - which page of 4 you're
  currently viewing.
- **Line below the badge**: the name of the active preset, if it's on
  the page you're currently viewing. If the name is wider than the
  screen, it scrolls left automatically (1px every 100ms) rather than
  getting cut off. If you've navigated to a *different* page than the
  one the active preset lives on, this line is blank - the footswitch
  LEDs are what actually tell you which of the 4 in the current page
  (if any) is active, not this line.

### What the LEDs mean

Each of the 4 preset LEDs lights up only when that footswitch's preset
on the *currently active* page is the one last sent to the amp. They
don't indicate anything about whichever page you happen to be
browsing/viewing if it's different from the active one - only the
OLED's preset-name line (or its blankness) tells you that.
