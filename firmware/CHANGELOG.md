# Changelog

All notable changes to the Dive WaterQuality Logger firmware are documented here.
Versions track `FW_VERSION` in [`src/shared.h`](src/shared.h) — the single source of truth.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

## [0.10.0] — 2026-07-15

### Added
- **DiveSync cloud offload** (`src/divesync.cpp`, docs/DiveSync-To-Do.md Phases 1/2/4): after a
  dive, on the surface, the logger joins a configured internet Wi-Fi and uploads unsynced
  `dive*.csv` files to Supabase on its own — raw CSV streamed to Storage, then a metadata row
  (parsed from the file's own `#` header) to PostgREST. Per the **revised, live-verified** cloud
  contract: plain POSTs, `409`/duplicate = already synced, FK `23503` = MAC not allowlisted
  (marked REJECTED, never retried forever). Publishable key on the `apikey` header only.
- **SETTINGS → "Data offload" card**: None (default — zero behavior change) / Cloud. Wi-Fi
  SSID + password, cloud URL + API key (blank = baked-in team defaults). Persists in
  `state.json` (`ds_*`), rides the unified POST-the-full-DOM save model.
- **On-SD sync manifest** (`/sync.csv`, append-only `filename,epoch,status`): survives reboots,
  prevents re-uploads. "Clear logs" wipes it too (dive numbering restarts after a clear).
  Upload objects are named `c<cast>_<file>` so a cleared card can never collide with an older
  upload of the same filename — the cast counter only climbs (now persisted at every log close).
- Footer shows **SYNC** while a pass runs; HELP gained an "Automatic cloud upload" topic.

### Safety / constraints
- **The dive loop is untouched.** The state machine advances only when `!g_logging &&
  !g_submerged`, portal AP down, run mode; any dive/portal activity — or a button press —
  aborts a sync instantly (Wi-Fi off well inside the 3-sample logging debounce).
- Non-blocking throughout (one SD→TLS chunk per `loop()` pass; sampling cadence unaffected),
  except two bounded, surface-only exceptions in the OTA spirit: the Wi-Fi join and the TLS
  handshake (deadline-capped). STA TX power lowered to the same brownout guard as the AP.
- TLS is `setInsecure()` at MVP (documented); hardening = pin the Supabase root CA.
- **Phase 3 (deep sleep) is deliberately NOT in this release** — sync gets field-validated
  first; a unit that never sleeps just behaves like v0.9.x.

## [0.9.4] — 2026-07-07

### Fixed
- **Boot-splash animation never played past the first frame** (and, on many real GIFs, hung the
  boot on a black screen). Root cause: the `gifRead`/`gifSeek` SD callbacks did not maintain
  `GIFFile.iPos`. AnimatedGIF **requires the callbacks to advance it** (its own built-in file
  callbacks do), so `iPos` stayed `0` and every frame after the first was re-parsed as the start of
  the file — frame 2 read mid-file bytes, failed the `GIF89` magic check, and returned
  `GIF_BAD_FILE`. Fixed by advancing `iPos` in both callbacks. Multi-frame splashes now play fully.
- **A malformed or trailing-byte GIF could wedge the boot forever.** The play loop treated
  `playFrame()`'s `-1` error return as "keep going" and had no upper bound. It now stops on any
  `rc <= 0` (end **or** error) and hard-caps frames (`SPLASH_MAX_FRAMES`) and wall-clock
  (`SPLASH_MAX_MS`), so no GIF can stall `setup()`. A truly corrupt file now falls back to the
  static logo as intended.

### Changed
- **Run-screen button gestures swapped** (field-feedback request): a **quick press now flips the
  page** (DIVE ↔ WATER) and a **hold-and-release now drops the POI marker**. Previously tap = POI,
  hold = page flip. The frequent action (paging) is now the easy tap; the rare, don't-do-it-by-
  accident action (dropping a marker) takes a deliberate hold. Only `runHandleNav()` changed — the
  calibration-wizard gestures (tap = capture/cycle, hold = cancel/select) are unchanged.
- The splash's final frame — and the static fallback logo — are now **held briefly**
  (`SPLASH_HOLD_MS`, 1.2 s) before the sensor self-test screen. Previously `bootSplash()` was
  followed immediately by `drawSensorScreen()`, so the hand-off image (and the fallback branding on
  a no-SD unit) was wiped in the same frame and never actually seen.

### Notes
- No new features and no partition/UI changes — this is a correctness release for the v0.9.3 splash
  player. The `/api/splash` OTA upload path and the "Splash / branding" SETTINGS card are unchanged;
  they worked all along, but uploaded GIFs only ever showed frame 1 until this fix.
- Splash authoring guidance unchanged: a **240×320** portrait GIF, short (~2–3 s). Encode with short
  per-frame delays (~12 fps) and limited colors — the C6 plays at roughly 6–7 fps off SD, so a GIF
  authored at 2 fps looks stepped and can bump the 8 s cap.

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

[0.9.4]: https://github.com/
[0.9.3]: https://github.com/
[0.9.2]: https://github.com/
[0.9.1]: https://github.com/
[0.9.0]: https://github.com/
[0.8.1]: https://github.com/
[0.8.0]: https://github.com/
