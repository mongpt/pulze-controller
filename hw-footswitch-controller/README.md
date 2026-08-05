# Pulze Footswitch Controller - Firmware

ESP32-S3-N16R8 firmware: 6 footswitches (4 presets + bank up/down), a
128x64 I2C SSD1306 OLED, 4 status LEDs. Connects directly to the Pulze
Mini over BLE (no phone/PC bridge needed) to recall exact saved patches,
and separately receives a curated preset library from the companion
web app over its own BLE connection.

## IMPORTANT - what has and hasn't been verified

This code has **not been compiled or run on real ESP32 hardware** from
where it was written (no ESP32 toolchain was available in that
environment). What HAS been genuinely verified, by actually compiling
and running it on a desktop machine:

- `TransferParser.cpp` (the logic that turns received bytes into
  Preset structs) - unit tested against synthetic data, including
  edge cases (oversized names, truncated/malformed input, capacity
  limits).
- The full JS-to-firmware wire protocol - a byte stream produced by
  literally simulating the web app's exact sending logic was fed into
  the real, compiled `parseTransferBuffer()` and round-tripped
  correctly.
- `PACKET3_TEMPLATE` and `PACKET_COMMIT` byte arrays in `config.h` -
  cross-checked byte-for-byte against the source hex strings (a real
  transcription bug was caught and fixed this way during development -
  worth knowing this class of error is exactly what this checking
  catches).

Everything else (NimBLE API calls, the Adafruit_SSD1306 display code,
GPIO/button timing, FreeRTOS task behavior) is written carefully
against the library APIs but is **unverified until flashed to real
hardware**. Expect a first-bring-up debugging pass, most likely around
exact NimBLE-Arduino API details if your installed library version
differs from what this was written against.

## Required libraries (Arduino Library Manager)

- **NimBLE-Arduino** (h2zero) - written against the 2.x API surface
- **Adafruit_SSD1306**
- **Adafruit_GFX**

Board: **ESP32S3 Dev Module** (esp32 boards package by Espressif)

## Required settings (Arduino IDE, Tools menu)

- **Partition Scheme**: pick one with a LittleFS partition sized for
  16MB flash (the default 4MB scheme is wrong for this board) - e.g.
  "16M Flash (3MB APP/9.9MB FATFS)" or a custom partitions.csv if you
  want to fine-tune the split.
- **PSRAM**: OPI PSRAM (this is the N16R8 variant - octal PSRAM)
- **USB Mode**: whichever matches how you're flashing/debugging (native
  USB is available on the S3 and is convenient for combined
  flash+serial-monitor over one USB-C cable)

## Wiring

See config.h for the full pin map and reasoning (avoids strapping
pins, the N16R8's octal-PSRAM-reserved pins, native USB D-/D+, and the
onboard RGB LED most S3 DevKitC boards wire to GPIO48). Verify against
your specific board's actual silkscreen before wiring, since GPIO
breakout availability varies between DevKitC revisions.

Footswitches: normally-open, one leg to the GPIO, other leg to GND.
Firmware uses INPUT_PULLUP, so no external resistor needed.

LEDs: GPIO -> ~220-330 ohm resistor -> LED anode -> LED cathode -> GND.

OLED: standard I2C SSD1306 module, SDA/SCL to the pins in config.h,
VCC/GND to 3.3V/GND. If it doesn't init, the two most common causes are
wrong I2C address (0x3C vs 0x3D - try both in Display.cpp) or SDA/SCL
swapped.

## How it works

**Two modes, chosen once at boot** (not switchable mid-session - see
the comments in PulzeFootswitch.ino for why):

- **Performance Mode** (default): connects to the amp as a BLE
  central, same role every other tool in this project has played.
  Bank Up/Down cycle through banks of 4; each footswitch recalls the
  preset in that bank/slot via the same packet2+packet3+commit
  sequence the Python/web versions use. The currently-active preset's
  LED stays lit until a different one is played.

- **Transfer Mode**: hold both Bank buttons for 2 seconds while
  powering on. The device becomes a BLE peripheral advertising as
  "Pulze Footswitch" - open the web app, tap "Transfer to Footswitch"
  in the Favorites panel, and pick it from Chrome's device dialog.
  Presets arrive in Favorites add-order and get assigned sequentially:
  1st -> bank0/fw1, 2nd -> bank0/fw2, 3rd -> bank0/fw3, 4th ->
  bank0/fw4, 5th -> bank1/fw1, and so on. Hold both Bank buttons again
  to cancel and return to Performance Mode (via a clean restart).

## Storage

- **LittleFS** (`/presets.bin`): the preset library itself. Only
  written once per transfer, not per button press - flash wear isn't
  a concern at that write frequency.
- **Preferences (NVS)**: the amp's remembered BLE address (for fast
  reconnect without rescanning) and the last-active preset index (so
  it survives a power cycle). Wear-leveled, safe to write on every
  preset change.

## Re-running the protocol tests

If you ever change the wire protocol (in config.h, TransferParser.cpp,
or the web app's app.js), re-verify both ends still agree:

```
cd host_test
g++ -std=c++17 -I.. -I. test_transfer_parser.cpp ../TransferParser.cpp -o test_transfer_parser
./test_transfer_parser
```

This compiles and runs the real firmware parsing logic on your
desktop machine - no ESP32 hardware required for this particular check.
