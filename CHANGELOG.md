# Changelog

All notable changes to the Dive WaterQuality Logger firmware are documented here.
Versions track `FW_VERSION` in [`src/shared.h`](src/shared.h) — the single source of truth.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

## [0.9.3] — 2026-06-30

### Added
- **Boot-splash player.** At power-on the unit plays `/splash.gif` from the SD card **one pass**,
  then hands off to the existing sensor self-test. Built on `bitbank2/AnimatedGIF`: the GIF is
  decoded **line-by-line** straight from SD (file callbacks `gifOpen/Close/Read/Seek`, no full
  framebuffer) and each scanline is converted to RGB565 and window-written to the ST7789
  (`gifDraw()`), honoring transparency runs and the restore-to-background disposal mode. The GIF's
  infinite-loop flag is ignored — playback stops after the final frame and holds it as the hand-off
  image. The decoder object is heap-allocated for the play and freed afterward, so it costs no RAM
  during a dive. Slotted into `setup()` after the sensor init, before `drawSensorScreen()`.
- **Static fallback logo (GFX primitives, no baked bitmap).** With no SD / missing / unreadable
  file the logger draws a ripple mark + "WATER QUALITY LOGGER" wordmark + the firmware version with
  `Adafruit_GFX`. It doubles as a diagnostic: drawn text instead of Kate's animation instantly
  signals a missing SD or `splash.gif`. Implemented as `drawSplashFallback()` in
  [`src/main.cpp`](src/main.cpp).
- **Over-the-air splash replacement** — `POST /api/splash` upload handler in
  [`src/setup_portal.cpp`](src/setup_portal.cpp), mirroring the OTA handler shape but writing to SD:
  stream chunks to `/splash.tmp`, sniff the `GIF8` magic, flush every ~16 KB (brownout pacing), and
  atomically rename `/splash.tmp → /splash.gif` only on a clean upload — a dropped connection can't
  replace a good animation with a truncated one. `{ok,err}` JSON reply mirrors `handleOtaDone`.
  **Apply = reboot** via the deferred `g_splashRebootAt` (≈800 ms, after the reply flushes).
  Registered in `portalBegin()` **only**, not the firmware-recovery AP (which has no SD).
- **"Splash / branding" SETTINGS card** in the captive portal ([`src/portal_page.h`](src/portal_page.h)) —
  a GIF file picker + `splashUpload()` `XMLHttpRequest` reusing the firmware-update progress UX, an
  iOS "use the full browser at `192.168.4.1`, not the captive popup" hint, and "rebooting…"
  messaging. Cache-then-upload, the same two-step flow as a firmware `.bin`.

### Changed
- `bitbank2/AnimatedGIF` added to `lib_deps`. App image ~62% of the ~1.94 MB OTA app slot.

### Notes
- This is an **SD file upload, not a flash OTA** — no `Update.h`, no slot swap, no brick risk, so
  no recovery path is needed for the asset; a corrupt upload's worst case is the static fallback.
- Asset target: a 240×320 portrait GIF, ~15 fps, kept short. The GIF lives on the SD card; it is
  **not** committed to the repo and **not** baked into flash (only the static fallback is built in).

## [0.9.2] — 2026-06-29

### Changed
- **Run-screen blanks now explain themselves.** When a POET channel (pH, ORP, EC, salinity)
  reads blank because it has not been calibrated, the corresponding tile shows an amber
  **`CALIBRATE`** prompt instead of a bare `--`, so divers understand the slot is empty
  because that metric needs calibrating — not because the sensor failed. The tile label
  (`pH`, `ORP`, `EC`, `SAL`) names the channel, so the tile reads e.g. "pH / CALIBRATE".
  Affects both the DIVE and DATA pages.

### Notes
- Display-only: an uncalibrated channel still **logs `NaN`** to CSV, and each channel still
  blanks independently (hard rule 6). The prompt only appears while the POET sensor is
  present and enabled; a disabled/absent sensor, or simply not-yet-submerged calibrated
  channels, still show a neutral `--`.
- The Cyclops fluorometer is unaffected — it already falls back to raw volts when
  uncalibrated rather than blanking.
- Implemented via `blankTile()` in [`src/main.cpp`](src/main.cpp).

## [0.9.1]

### Added
- In-browser dive-log charts in the portal Download view (client-side SVG small-multiples).

## [0.9.0]

### Added
- Per-sensor enable toggles + I²C auto-detect (POET, BAR30, Cyclops ADS, Celsius), each with
  a live detected/absent frame in SETTINGS and a `GET /api/scan` re-scan endpoint.
- Blue Robotics Celsius (TSYS01) high-accuracy temperature sensor support; adds the
  `cels_T_C` CSV column and a TEMP-tile source preference of Celsius → BAR30 → POET.

### Changed
- BAR30/POET CSV columns are blank (not numeric) when those sensors are disabled — downstream
  parsers should treat empty as N/A.

## [0.8.1]

### Added
- Firmware-update HELP topic in the portal.

## [0.8.0]

### Added
- OTA firmware update ("lone-diver file-mule" flow): `POST /api/ota` streaming upload, layered
  pre-commit image validation (`otaHeaderOk()`), on-device "DO NOT POWER OFF" progress screen,
  and the boot-gesture recovery AP for sealed-unit re-flash.

[0.9.3]: https://github.com/
[0.9.2]: https://github.com/
[0.9.1]: https://github.com/
[0.9.0]: https://github.com/
[0.8.1]: https://github.com/
[0.8.0]: https://github.com/
