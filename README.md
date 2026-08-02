# Pulze // Control - standalone Android build

No backend, no Python, no PC - this version talks to the amp directly
from JavaScript using the Web Bluetooth API. Your preset library lives
in the browser's own storage instead of a server file.

## Read this first - real constraints, not just fine print

- **Chrome (or Edge/Brave) on Android only.** Firefox for Android and
  Samsung Internet don't support Web Bluetooth. iOS doesn't support it
  in any browser (Apple has never shipped it, and App Store rules force
  every iOS browser to use Apple's engine underneath).
- **Needs a "secure context" to even access Bluetooth at all.** A page
  opened by just tapping the .html file directly will likely be blocked
  from using Bluetooth. See "How to open it" below for options that work.
- **The 178-byte preset-load write's reliability is genuinely untested
  against your amp from here.** Small messages will work fine regardless.
  If "Play" fails while everything else works, that's almost certainly
  Android/Chrome not negotiating a large enough Bluetooth MTU - Web
  Bluetooth gives JavaScript no way to request or check this explicitly,
  unlike the native code the Python version used. There's no code fix
  for this from our side if it turns out to be the issue - it would be a
  platform limitation.

## How to open it

Pick whichever is easiest for you:

**Option A - host it online (simplest ongoing use).**
Upload `index.html`, `style.css`, and `app.js` to any static host - e.g.
GitHub Pages (free): create a repo, add these 3 files, enable Pages in
the repo's Settings. You'll get a `https://yourname.github.io/...` URL
that works from Chrome on any device, and you can add it to your
phone's home screen for a one-tap launch.

**Option B - serve it from the phone itself, fully offline.**
Install [Termux](https://f-droid.org/packages/com.termux/) (from
F-Droid, not the outdated Play Store version), then:

```
pkg install python
cd /path/to/these/files
python -m http.server 8000
```

Open Chrome and go to `http://localhost:8000`. `localhost` counts as a
secure context, so Bluetooth will work. Leave Termux running in the
background while you use the page.

**Option C - just try opening the file directly.**
Double-tap `index.html` to open it in Chrome. This *might* work
depending on your Chrome version's exact policy for `file://` origins,
but it's the least reliable option - if the Connect button does
nothing or throws an error, use Option A or B instead.

## Using it

Same as the desktop version, with one difference: there's no "Scan for
amps" button or device dropdown. Instead, tapping **Connect to amp**
opens Chrome's own built-in Bluetooth device picker (a native Android
dialog, not part of this page) - pick your Pulze from that list. This
is a Web Bluetooth security requirement: pages can't silently scan for
or list nearby devices themselves, only trigger the browser's own
picker in response to a tap.

Everything else works the same as before:

1. Tap a patch tile to play it - it turns green while it's the one
   currently applied.
2. Change a tone on the amp (app, footswitch, or the amp itself) -
   "snapshot ready to save" lights up once captured.
3. Name it, choose a slot, tap **Save patch**.

## Backing up your library

Since there's no server-side JSON file anymore, your presets live in
this browser's local storage on this device only - clearing browser
data, switching browsers, or moving to a new phone would lose them.
Use the **Export library** / **Import library** buttons to save a
`pulze_presets.json` file you can back up or move to another device -
same file format as the Python version used, so they're interchangeable.

Your existing 16 patches from the Python version are already built in
as the starting library the first time you open this page.
