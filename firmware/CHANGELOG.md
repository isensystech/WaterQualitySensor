# Changelog

All notable changes to the Dive WaterQuality Logger firmware are documented here.
Versions track `FW_VERSION` in [`src/shared.h`](src/shared.h) — the single source of truth.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

## [0.10.9] — 2026-07-20

### Fixed
- **A dive whose file uploaded but whose metadata row didn't could never recover** — it retried
  forever *and head-of-line blocked every newer dive behind it* (field: dive0003 wedged the queue
  through 4 backoff cycles while dive0004–0007 waited). Two real bugs:
  - **Response buffer was smaller than the response headers.** `s_resp` was 600 B; Supabase behind
    Cloudflare returns ~790 B of headers (a ~300 B `set-cookie` alone) before the JSON body. The
    buffer filled with headers, so the body markers the logic keys on were never in RAM. Buffer is
    now 1600 B.
  - **Duplicate detection keyed off the status line.** Storage answers an already-uploaded object
    with **HTTP 400**, carrying the real `"statusCode":"409","error":"Duplicate"` *in the body*.
    Storage and metadata responses are now matched on body markers (`Duplicate`/`already exists`,
    PostgREST `23505`/`23503`) independent of the status line, so a re-POST of an existing object
    correctly proceeds to writing the missing metadata row.
- Storage/metadata failures now record the **actual HTTP code and body snippet** to `/synclog.csv`
  instead of a blank "storage http error".
- **Head-of-line guard:** a file that fails `DSYNC_MAX_FILE_FAILS` (3) times in a row is deferred
  for the rest of the power cycle so the queue keeps draining; a reboot clears the list.

## [0.10.8] — 2026-07-20

### Fixed
- **TLS handshake failed right after a good join.** `startFile()` dropped TX power to 13 dBm
  immediately before the handshake, on a link that had just associated at full power — too weak
  through the sealed housing (intermittent "failed: tls connect"). The whole sync now stays at
  full TX; lower `DSYNC_TX_PUMP` only if a brownout ever appears.
- **DNS is split from the handshake** in failure text, with RSSI and free heap, so a name-resolution
  failure, a dead link and a memory problem are no longer one indistinguishable "tls connect".
- **`/api/wifitest` now probes DNS + TLS to the cloud** after associating, so the entire upload path
  is verifiable from the portal without burning a dive cycle per attempt.

## [0.10.7] — 2026-07-17

### Changed
- **Data offload card reworked** after a field UX trap: the old card had a "Scan" dropdown *above*
  a separate SSID text box, and picking from the dropdown silently did nothing unless you also
  retyped the name below — so a saved config could end up with a password but a blank SSID, which
  keeps the sync gate shut with no obvious cause. Now the **network dropdown IS the SSID**: it
  auto-scans when the Settings tab opens (reads "Please wait…" while scanning), picking a network
  sets it directly, and an "Enter manually…" row covers a network that isn't currently broadcasting.
- **"Test this Wi-Fi now" button** (`GET /api/wifitest`, result in `/api/state` `ds_test`): the
  logger AP_STA-joins the chosen network and reports **OK (joined, with IP)**, **password/security
  rejected**, or **timeout** — so credentials are verified on the spot instead of only failing
  silently after a dive. Saving changed credentials also kicks the test in the background. (Single
  radio: the ~12 s test can briefly drop the phone from the portal; the result is stored and the
  page reads it back on reconnect.)
- **Expert-mode toggle** at the top of Settings hides advanced controls by default — Cloud
  URL/API key, Firmware update, Splash/branding, and Enter-calibration are shown only when it's on.
- **Offload type** is now None / Local / Cloud; **Local** is selectable (persists) but inert until
  the base station ships (`dsAllowed()` still gates on Cloud only).

## [0.10.6] — 2026-07-17

### Fixed
- **DiveSync could never join Wi-Fi from inside the sealed housing.** The STA join reused the AP's
  `WIFI_POWER_8_5dBm` brownout guard, set the instant `WiFi.begin()` was called. The unit could
  *receive* the router (saw the SSID in the scan at -70 dBm) but its 8.5 dBm reply couldn't get
  back out through the tube, so every join ran the full 15 s `DSYNC_CONNECT_MS` and timed out —
  while bare ESP32s on the same network joined fine (confirming the network, not the credentials).
  Association now uses full power (`DSYNC_TX_JOIN` = 19.5 dBm); the sustained upload eases back to
  a mid level (`DSYNC_TX_PUMP` = 13 dBm) that still clears the housing but keeps brownout margin.
- **"join failed - check password" was a mislabel** — it fired on a plain association timeout as
  well as a real credential reject, sending us chasing a correct password. The two are now split:
  `auth rejected …` only on `WL_CONNECT_FAILED`, else `join timeout, no association (wl_status N)`
  with the raw status code. On success the joined **IP and RSSI are logged** to `/synclog.csv`.

## [0.10.5] — 2026-07-17

### Added
- **DiveSync is now debuggable on a *sealed* unit.** The v0.10.4 status line lives in RAM and is
  served by the portal — but the portal is torn down for the entire sync attempt, and a tube unit
  has no serial and no reachable SD, so a field sync that fails leaves no trace you can read. Three
  durable records now close that gap:
  - **Persistent `/synclog.csv`** (append-only `epoch,ms,event`, survives reboot, self-rotates at
    16 KB): one line when an attempt starts (`scanning for 'X'`), and one at each outcome —
    `'X' NOT seen`, `FAILED: join failed …`, `dive0003.csv -> OK/REJECTED`, `pass complete`.
    Written only at quiescent points (radio off or idle) to respect the Wi-Fi+SD brownout rule.
  - **On-screen last-sync status** painted just above the footer on the run screen, colour-coded
    (cyan busy / red failed / green synced). It persists after a failure — e.g. `failed: join
    failed - check password` stays visible through the tube until the next retry.
  - **`GET /api/diag`** dumps `sync.csv` + `synclog.csv` as plain text, so the whole history is
    readable over the portal the moment the AP returns after a sync attempt.
- Rationale: first live field sync uploaded nothing and there was no way to see why on the actual
  hardware. This makes the unit self-reporting instead of needing a bench + USB to diagnose.

## [0.10.4] — 2026-07-16

### Added
- **DiveSync is now field-debuggable.** First live test uploaded nothing and gave no clue why —
  the "scanned, target SSID not seen" path (the most common field failure: 5 GHz-only hotspot,
  hotspot gone dormant with its screen off, SSID typo/curly-quote) was completely silent.
  - Serial now prints every scan outcome, including the full list of networks seen (with RSSI)
    when the target is missing.
  - The portal's **Data offload card shows a live one-line sync status** (`/api/state` `ds_stat`):
    "'X' not seen (6 nearby)", "join failed - check password", "uploading dive0003.csv",
    "synced 2 file(s)", "dive0003.csv REJECTED - MAC not on allowlist", …
  - **"Scan — show networks the logger sees"** button (`GET /api/wifiscan`, AP_STA keeps the
    portal up; ~2 s blocking scan under the user-initiated rule-5 exception): pick the SSID from
    the list instead of typing it — and if a network isn't listed, the logger can't join it,
    which answers the band/dormancy question on the spot. Hotspot tips added to the card.

## [0.10.3] — 2026-07-16

### Fixed
- **Builds without a POET never sampled at all.** The sample cycle only advanced when
  `poetStart()` got an I2C ACK, so with no POET on the bus the state machine sat in `S_IDLE`
  forever: no sensor reads, no log rows (a "dive" file held only its header), screen values
  frozen at boot defaults (TEMP showed `0.0` from the never-updated BAR30 default), submersion
  never detected, and — because the portal teardown lives in the sample path — the AP never
  dropped, which also hard-blocked DiveSync. This masked both the v0.10.1 depth trigger and the
  v0.10.2 manual override on exactly the variants they were built for. The cycle now advances
  unconditionally each `SAMPLE_MS`; the POET kick is attempted only when POET is enabled, and a
  NACK just blanks that sample's POET columns. `g_poetOk` is likewise gated on `poet_en` so a
  disabled-but-present POET can't serve stale data.

### Changed
- **Displayed temperature source of truth** now follows the fleet build rules: **POET when
  fitted → Celsius → BAR30** (was Celsius → BAR30 → POET). Salinity/EC compensation is
  unchanged — it always uses POET's own temperature.

## [0.10.2] — 2026-07-16

### Added
- **Manual logging override — hold the button 3 seconds** to force logging on or off, for bench
  tests and deployments without enough water column for the automatic trigger. A `LOGGING ON` /
  `LOGGING OFF` banner confirms the toggle and the footer shows an amber `LOG*` while forced.
  Semantics:
  - Forcing a log on behaves like a real dive (portal Wi-Fi drops; DiveSync stays gated until it
    ends), and **hands back to the automatic gate once real submersion is detected**, so a
    forced-on log still closes — and cloud-syncs — by itself at the surface.
  - Forcing a log off resets the wet streak, so a unit still sitting in water gets a full
    debounce (~12 s) of grace before the auto-gate reopens a fresh log.
  - Holding while already logging drops a POI at 0.9 s on the way to the 3 s stop (the marker is
    harmless); a press that only wakes a dimmed screen is swallowed whole — it can't toggle
    logging.
- HELP: the push-button table, status-line legend, dive topic and troubleshooting now cover the
  3-second hold and the `LOG*` footer flag.

## [0.10.1] — 2026-07-16

### Fixed
- **BAR30+Celsius-only variants never started a dive log.** Submersion was detected solely from
  the POET's EC current, so with no POET fitted `g_submerged` could never go true. The gate now
  accepts either wet signal: POET EC above `EC_SUBMERGED_NA` (when fitted **and enabled**), or
  BAR30 depth beyond `DEPTH_SUBMERGED_M` (0.5 m — above the ~0.3 m a unit in air can read under
  a strong high-pressure system). Side effects: a mid-dive POET read glitch no longer ends a
  logging run while the unit is at depth, a disabled-but-fitted POET no longer drives the
  trigger, and the DEPTH tile now shows during dives on POET-less variants.

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
