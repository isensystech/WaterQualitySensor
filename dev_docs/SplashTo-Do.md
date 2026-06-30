# SplashTo-Do.md — Splash player + asset OTA delivery

> **✅ Shipped in firmware v0.9.3** (2026-06-30). Both phases are implemented: the AnimatedGIF
> boot-splash player + GFX static fallback (`bootSplash()`/`playSplashGif()`/`drawSplashFallback()`
> in `main.cpp`) and the `POST /api/splash` upload handler + "Splash / branding" SETTINGS card
> (`setup_portal.cpp` / `portal_page.h`). This file is retained as the design record. See the
> "Boot splash" note in `CLAUDE.md` and the `0.9.3` entry in `CHANGELOG.md`.

Working spec for the **boot-splash animation player** and for **uploading a new splash over
the AP**, so a sealed unit never needs the SD card pulled. Companion to `CLAUDE.md`; same hard
rules apply.

**Two phases, build in order:**
1. **Player** — play `/splash.gif` from SD at boot (AnimatedGIF), one pass, static fallback.
   *Does not exist yet — build first.*
2. **OTA delivery** — a `/api/splash` upload handler that writes the GIF to SD, plus a
   **"Splash / branding"** card in SETTINGS with a file picker, mirroring the firmware-update
   (`/api/ota`) card/handler. **Apply = reboot** (locked).

The upload in Phase 2 simply replaces the file the Phase 1 player reads, so the player is the
foundation — get a splash playing from SD first, then make it replaceable over the air.

---

## The one fact that makes this easy (and safe)

**This is a file upload to SD, not a flash OTA.** None of the `Update.h` / slot-swap /
validate-before-commit / recovery-AP machinery applies. The SD is already mounted and
writable, so delivery is just: receive HTTP chunks → stream to `SD.open("/splash.gif",
FILE_WRITE)`. No partition, no brick risk. A corrupt upload's worst case is the splash
doesn't decode → the **baked-in static logo fallback** plays. So no recovery path is needed
for this asset.

Reuses plumbing you already have: the `WebServer` `HTTPUpload` chunked-callback pattern from
`/api/ota`. Only the destination differs — OTA feeds chunks to `Update.write()`; splash feeds
them to `file.write()`.

---

## Delivered asset (Kate, verified 2026-06-30)

`WLCSplash.gif` — fade-in ripples → koru mark → **PlanetWerx / WATER QUALITY LOGGER** lockup.

| Property | Value | Note |
|---|---|---|
| Dimensions | **240×320 portrait** | matches panel exactly ✓ |
| Frames | 125 | |
| Frame delay | 40 ms | **= 25 fps** as delivered → **Kate re-exporting at 15 fps** |
| Loop length | 5.0 s | |
| Loop flag | 0 (infinite) | **player plays ONE pass then hands off** — ignore the flag |
| File size | ~918 KB | fine for SD and for chunked upload; 15 fps export will be smaller |
| Colors | ~30 in peak frame | low; RGB565 banding risk minimal |

**Asset notes:** delivered at 25 fps; **Kate is re-exporting at 15 fps** (fewer frames,
smaller file, more decode headroom). The last ~45 frames are the *identical* held logo
(~1.8 s) — trimmable, since the player holds the last frame / moves on regardless. Expect a
fresh `WLCSplash.gif` to land before player testing.

---

## Phase 1 — Player (build first)

Plays `/splash.gif` from SD at boot, **one pass**, then hands off to the existing boot sensor
self-test. No SD / no file / decode fail → **static fallback logo**.

- **Library:** `bitbank2/AnimatedGIF` (Larry Bank). Add to `lib_deps`. Portable C — should
  build on the C6/RISC-V; flag any build flag it needs. Decodes **line-by-line via a draw
  callback** (no full framebuffer), so RAM cost is small — fits the C6 comfortably.
- **SD source:** use AnimatedGIF's **file callbacks** (`open`/`close`/`read`/`seek`) to decode
  straight from the SD file — do not load the whole GIF into RAM.
- **Blit:** in the GIFDRAW callback, convert the decoded scanline to RGB565 and push it to the
  ST7789 (`Adafruit_ST7789` windowed write). Honor each frame's delay (the lib returns it).
- **One pass only:** play frames until one full iteration completes, then stop — **ignore the
  GIF's infinite-loop flag.** Hold the final frame as the handoff image.
- **Boot placement:** in `main.cpp` `setup()`, after display init, before/around the boot
  self-test. **Blocking play is fine here** — boot/setup is the one place `delay()` is allowed
  (CLAUDE.md rule 5); the no-blocking-in-run-path rule does not apply to the splash.
- **Static fallback — GFX primitives (locked).** For the no-SD / missing / corrupt case,
  draw the mark + "WATER QUALITY LOGGER" wordmark with `Adafruit_GFX` primitives + text — no
  baked bitmap. Two reasons: near-zero flash, and it **doubles as a diagnostic** — booting to
  plain drawn text instead of Kate's animation instantly signals a missing SD / `splash.gif`.
  A pixel-perfect bitmap fallback would look identical to success and mask the failure.
  (Kate's 240×320 PNG is kept as a source still / last-frame reference, **not** baked in.)

---

## Phase 2 — Endpoint contract — `/api/splash` (mirror `/api/ota`)

Mirror the real handlers in `setup_portal.cpp` (verified 0.9.2): **`handleOtaUpload`** (the
`HTTPUpload` chunk callback) and **`handleOtaDone`** (the completion reply).

- **Method:** `POST`, `HTTPUpload` chunked — same callback shape as `handleOtaUpload`, but
  feed chunks to a `File` write instead of `Update.write()`.
- **Target:** **write to `/splash.tmp`, then rename to `/splash.gif` only on a clean
  `UPLOAD_FILE_END`.** On `UPLOAD_FILE_ABORTED`, delete the tmp. Atomic-ish swap — a dropped
  connection can't replace a good animation with a truncated one.
- **Optional validation:** sniff `GIF87a` / `GIF89a` magic before the rename. The static
  fallback covers a bad file regardless.
- **No big RAM buffer** — stream chunk-by-chunk straight to the file.
- **Done reply:** mirror `handleOtaDone`'s JSON contract — `{"ok":true}` on success,
  `{"ok":false,"err":"..."}` on failure — so the card's JS can reuse the OTA progress UX.
- **Apply = reboot (locked), via the house deferred-reboot pattern.** Don't call
  `ESP.restart()` inline. On success, set a `g_splashRebootAt = millis() + 800` (mirroring
  `g_otaRebootAt`) and let `loop()` restart after the reply flushes.
- **Register in `portalBegin()` only** — NOT `portalBeginRecovery()`. The recovery AP is
  firmware-only with no SD mounted; a splash upload there has nothing to write to.
- **Scope:** splash-only for now. Generalizes to any SD asset later, but don't until there's a
  second concrete use.

**Depends on Phase 1.** This handler replaces the `/splash.gif` the player reads, so the
player must be working first — otherwise there's nothing to test the upload against.

---

## SETTINGS card — "Splash / branding"

In `portal_page.h`, clone the existing **Firmware update** card (`<div class=c><h3>Firmware
update</h3>` … `otaUpload()` … `#otawrap`/`#otafill`/`#otamsg` progress). The splash card:

- `<div class=c><h3>Splash / branding</h3>` with `<input type=file id=splashfile
  accept="image/gif">` + a `<button class=b2 onclick=splashUpload()>Upload splash&hellip;</button>`.
- A `splashUpload()` JS fn mirroring `otaUpload()` — POST to `/api/splash`, reuse the same
  progress wrap / `{ok,err}` handling.
- On `{"ok":true}`, show "rebooting…" and let the unit restart (apply = reboot).
- Helper text: 240×320 GIF, ~15 fps, keep it short; falls back to the built-in logo if missing.
- iOS hint (see below). Keep the ABOUT credits block verbatim.

---

## Constraints / gotchas

1. **iOS captive-portal popup blocks file pickers** — same caveat as OTA. iOS users must do
   this from the full browser at `http://192.168.4.1`, not the CNA popup. Surface that hint
   on the card.
2. **Brownout** — writing a ~1 MB file to SD with WiFi up is the same brownout-sensitive op as
   `state.json` saves, already mitigated by `WIFI_POWER_8_5dBm`. **Flush periodically** during
   the larger write; don't raise TX power.
3. **Phone-as-mule** — SoftAP ↔ internet are mutually exclusive, so the user gets the GIF onto
   the phone *first* (email / link / AirDrop), then connects to the AP and uploads.
   Cache-then-upload, same as the `.bin`.

---

## Tasks (ordered)

**Phase 1 — Player**
1. Add `bitbank2/AnimatedGIF` to `lib_deps`; confirm it builds on the C6.
2. Wire AnimatedGIF **SD file callbacks** (open/close/read/seek) to decode `/splash.gif`.
3. GIFDRAW callback → RGB565 → ST7789 windowed blit; honor per-frame delay.
4. **One-pass** play loop (ignore infinite-loop flag); hold final frame on exit.
5. Slot into `main.cpp` `setup()` after display init, before/around the boot self-test.
6. **Static fallback (GFX primitives, locked)** — draw mark + "WATER QUALITY LOGGER" wordmark
   with `Adafruit_GFX` for the no-SD / missing / corrupt case. No baked bitmap.

**Phase 2 — OTA delivery**
7. **`/api/splash` handler** in `setup_portal.cpp` — clone `handleOtaUpload`'s chunk callback
   but write to `/splash.tmp`; rename to `/splash.gif` on `UPLOAD_FILE_END`, delete tmp on
   abort. Add a `handleSplashDone` mirroring `handleOtaDone`'s `{ok,err}` reply.
8. **Magic-byte check** (optional) — verify `GIF8` header before the rename.
9. **Register route** in `portalBegin()` only (`server.on("/api/splash", HTTP_POST,
   handleSplashDone, handleSplashUpload)`); **not** in `portalBeginRecovery()`.
10. **SETTINGS card** in `portal_page.h` — clone the Firmware-update card (`splashfile` /
    `splashUpload()`), reuse the OTA progress UX. Keep the credits block verbatim.
11. **Reboot-to-apply** — set `g_splashRebootAt = millis()+800` on success; restart in `loop()`.
12. **iOS hint + helper text** on the card.

---

## Hard rules (inherited from CLAUDE.md — do not break)

1. **Credits block verbatim** in any `portal_page.h` edit.
2. **HTML/JS stays in `portal_page.h`** — never inline into `setup_portal.cpp`.
3. **No partition change** — this is an SD write, nothing touches flash partitions.
4. **Don't raise WiFi TX power** (`WIFI_POWER_8_5dBm`).

---

## Definition of done

**Phase 1:** the unit plays `WLCSplash.gif` from SD at boot, one pass, then continues to the
self-test; a missing/corrupt file falls back to the static logo. **Phase 2:** a user can pick
a GIF in the SETTINGS "Splash / branding" card, upload it over the AP, and on auto-reboot the
new animation plays — with an atomic tmp→rename so a dropped upload can't corrupt the live
splash, and no SD card removal on a sealed unit.

---

## Pre-flight (verified against 0.9.2 — 2026-06-30)

- ✅ **OTA handler present** to mirror: `handleOtaUpload` + `handleOtaDone` in
  `setup_portal.cpp` (`HTTPUpload`, `Update.*`, `{ok,err}` JSON, deferred reboot via
  `g_otaRebootAt`). Clone its exact shape for `/api/splash`.
- ✅ **Firmware-update card present** to mirror in `portal_page.h` (`#fwfile`, `otaUpload()`,
  `#otawrap`/`#otafill`/`#otamsg`).
- ✅ **No splash code exists yet** — Phase 1 player confirmed greenfield.
- **Boot sequence:** find the `main.cpp` `setup()` order (display init → sensor self-test) to
  place the one-pass splash play.
- Confirm the player's read path (`/splash.gif`) matches the upload handler's write target.
