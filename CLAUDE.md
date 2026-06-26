# CLAUDE.md — Dive WaterQuality Logger

Firmware for a dive-deployable water-quality logger. iSENSYS, for client Fieldwerx.
Field users are divers and STEM students/educators. The unit is **watertight-sealed** —
assume no USB access in the field; that single fact drives most of the constraints below.

## Build / flash / monitor (PlatformIO)

```bash
pio run                       # build
pio run -t upload             # flash over USB (close the monitor first — see gotchas)
pio device monitor            # serial, 115200, over the C6 native USB
pio run -t clean              # clean
```

- Platform is the **pioarduino** fork, pinned in `platformio.ini`. The *official*
  `platformio/espressif32` platform does **not** support the Arduino framework on the
  ESP32-C6 — do not switch to it.
- Target board: `seeed_xiao_esp32c6` (4 MB flash, 320 KB RAM, RISC-V).
- App currently ~63% of the ~1.94 MB OTA app slot (`app0`/`app1`, `0x1F0000` each).
- The OTA app image is `.pio/build/seeed_xiao_esp32c6/firmware.bin` (the app-partition
  image, **not** a merged/factory bin). This is the file the file-mule OTA flow uploads.
- **Releases are published as GitHub Release assets, not committed.** Both `.pio/` and
  `builds/` are gitignored; `builds/` is only a local staging dir for the named bins. Cutting a
  release: build, copy the two products into `builds/` with the names below, tag the commit
  `v<VER>`, and upload them to a GitHub Release
  (`gh release create v<VER> builds/firmware-v<VER>.bin builds/WaterQuality-<VER>-flash-at-0x0.bin`):
  - `firmware-v<VER>.bin` &larr; `firmware.bin` (the **OTA** app image the away team uploads)
  - `WaterQuality-<VER>-flash-at-0x0.bin` &larr; `firmware.factory.bin` (full USB flash / recovery seed)

## Hardware

| Part | Bus / pin | Notes |
|---|---|---|
| Seeed XIAO ESP32-C6 | — | MCU, 4 MB flash |
| Adafruit 2.0" ST7789 TFT | SPI | 240×320, portrait |
| Blue Robotics BAR30 (MS5837) | I²C `0x76` | depth/pressure/temp |
| POET electrochemical sensor | I²C `0x1F` | pH (ISFET), ORP, conductivity, salinity, temp |
| SparkFun ADS1015 ADC | I²C `0x48` | optional Turner Cyclops-7F fluorometer (0–5 V) |
| Blue Robotics Celsius (TSYS01) | I²C `0x77` | optional high-accuracy temperature |
| microSD | SPI | logging |
| Twist-actuator momentary button | `D0` | **the only physical input** |

All four I²C sensors are individually enable-able and auto-detected (see the sensor-management
roadmap note). Pin defines live in `shared.h` (`PIN_*`). I²C address defines: `BAR30_ADDR`,
`CELS_ADDR`, plus `0x1F`/`0x48` for POET/ADS.

## Source layout (`src/`)

- **`main.cpp`** — `setup()`/`loop()`, boot sensor self-test, backlight PWM + auto-dim,
  sensor sampling, submerge/logging gate, run-screen rendering (DIVE / DATA pages).
- **`shared.h`** — all cross-file prototypes, includes, `#define`s, globals.
  `FW_VERSION` is defined **canonically here** (currently `0.9.0` — `0.8.0` added OTA, `0.8.1`
  added the firmware-update HELP topic, `0.9.0` added per-sensor enable toggles + I2C auto-detect
  and the Blue Robotics Celsius (TSYS01) sensor).
- **`calibration.cpp`** — on-device cal flows (pH 3-pt, EC 1-pt, ORP 1-pt, Cyclops 2-pt)
  and salinity/PSU math. Entered via boot twist-hold or the portal.
- **`setup_portal.cpp`** — SoftAP captive portal: `WebServer(80)`, `DNSServer`,
  `/api/*` REST endpoints, `state.json` + `cal.json` persistence.
- **`portal_page.h`** — the single-page web app (HTML/CSS/JS) as one PROGMEM string.

SD files: `state.json` (settings/mission/time/thresholds), `cal.json` (calibration),
`dive*.csv` (logs), `callog.csv` (calibration audit log).

## Hard rules — do not break these

1. **Credits block is verbatim.** The portal's ABOUT credits (iSENSYS / Fieldwerx, the
   full team, © 2026) must be preserved exactly in any `portal_page.h` edit.
2. **HTML stays in `portal_page.h`.** Never inline the page into `setup_portal.cpp`.
3. **Partition table is locked.** `partitions_ota.csv` (dual OTA slots, no SPIFFS) must
   not change — a sealed unit can't be re-partitioned without USB. It is **already
   OTA-ready** (`otadata` + `app0`/`app1`); OTA needs zero partition change — that is the
   whole reason this layout was committed up front.
4. **One physical input.** All on-device UI/gestures must work with the single `D0`
   twist button. No touch, no added buttons.
5. **No blocking in the run path.** Use `millis()` state machines, not `delay()`
   (boot/setup is the only place `delay()` is acceptable). The OTA upload is a deliberate,
   bounded exception: `WebServer::handleClient()` blocks for the whole transfer, so
   sampling/UI pause for the duration. That is acceptable **only** because OTA is a
   surface, user-initiated, not-logging action — never reachable during a dive.
6. **Sensor blanking is independent.** ORP (`cal.orp_valid`) and EC/salinity
   (`cal.ec_valid`) blank separately; uncalibrated → display `--`, log `NaN`. Fix and
   verify each independently, never as one change.
7. **Unified save model.** START MISSION and SETTINGS both POST the full DOM to
   `/api/deploy`; neither wipes the other's fields.

## Architecture constraints worth knowing

- **SoftAP ↔ internet are mutually exclusive on the phone.** The phone can't be on the
  logger's AP and the internet at once. All data-offload / OTA design is **cache-then-
  upload**, never simultaneous transfer.
- WiFi TX power is intentionally lowered (`WIFI_POWER_8_5dBm`) to avoid WiFi+SD brownout
  resets during saves. Don't raise it without retesting.
- The portal is torn down once a dive starts (`g_logging`) — WiFi is surface-only.

## Gotchas

- **Upload "Resource busy" / can't open port** → a serial monitor is holding it. Close
  the PlatformIO monitor task and re-upload. (Last resort: BOOT+RESET to force download mode.)
- **Blank serial monitor** → flip `ARDUINO_USB_CDC_ON_BOOT` to `0` in `platformio.ini`.
- BlueRobotics MS5837 is pulled from GitHub in `lib_deps` (registry name is unreliable).
- **OTA brownout risk** → flash writes + WiFi RX draw current; treat OTA like the save path.
  Keep TX power low (`WIFI_POWER_8_5dBm`) and never run OTA alongside SD activity (it isn't
  logging on the surface, so this holds naturally). Don't kill the backlight to save current
  — the user must read the "DON'T POWER OFF" warning; draw a mostly-dark progress screen
  (low panel current, still legible) instead.
- **No auto-rollback compiled in** → the stock pioarduino core does not enable
  `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, so a validated-but-buggy image that boots will
  stick. Defences are pre-commit validation (`Update.h` header/size check) + the recovery
  AP gesture. An image that fails to boot *at all* has no software escape — validate hard.

## Feature notes & roadmap

The two big subsystems below are **shipped** (v0.8.x OTA, v0.9.0 sensor management); the notes
stay here because they document non-obvious design choices. RTC is the only open roadmap item.

- **Sensor management (done in v0.9.0)** — every I2C sensor (POET `0x1F`, BAR30 `0x76`, Cyclops
  ADS `0x48`, Blue Robotics Celsius/TSYS01 `0x77`) now has its own SETTINGS card with an enable
  toggle and a live green/red "detected" frame. Boot does an I2C presence scan **before**
  `stateLoad()` so an unconfigured sensor defaults to *detected = enabled*; saved user choices
  override and persist (`poet_en`/`bar30_en`/`cels_en`/`cyc_en` in `state.json`). A disabled (or
  absent) sensor **blanks independently** (rule 6): `--` on screen, empty CSV columns, grey "off"
  in the boot self-test instead of red "FAILED". Portal pieces: `GET /api/scan` (re-ping for the
  "Re-scan sensors" button) and a `det{}` block in `/api/state`. **Celsius** is a small inline
  TSYS01 driver in `main.cpp` (two-phase like POET — conversion kicked at sample start, read in
  `sampleFinish()`, so no run-path blocking); it adds a trailing **`cels_T_C`** CSV column and a
  `# sensors:` provenance line, and the TEMP tile now prefers Celsius → BAR30 → POET (salinity/EC
  math still uses POET's own temp). **CSV note:** BAR30/POET columns are now blank when those
  sensors are disabled (previously always numeric) — downstream parsers should treat empty as N/A.

- **OTA (shipped v0.8.0; firmware-update HELP topic added v0.8.1)** — "lone-diver file-mule"
  path: on the surface, a phone/laptop that *already downloaded* `firmware.bin` over the
  internet joins the logger AP and uploads it; the logger flashes its inactive slot and reboots.
  SoftAP↔internet exclusivity means the download and the upload are always two separate steps
  (cache-then-upload). How it works:
  - **`POST /api/ota`** streaming upload handler in `setup_portal.cpp` — `WebServer`'s
    `on(path, HTTP_POST, handleOtaDone, handleOtaUpload)` form + `Update.h`:
    `Update.begin(UPDATE_SIZE_UNKNOWN)` → `write()` per chunk → `end(true)`. Refuses while
    `g_logging`. Image size arrives via a `?size=` query so the on-device bar can show %. After
    a good flash it arms `g_otaRebootAt` (≈800 ms out) and `loop()` calls `ESP.restart()` once
    the reply has flushed.
  - **Pre-commit validation** is `otaHeaderOk()` (`setup_portal.cpp`) — a layered header check,
    strongest first: image magic `0xE9` → **`chip_id == ESP32-C6` (0x000D, image byte 12)** →
    app-descriptor magic (`0xABCD5432`, byte 32) → `project_name` (byte 80) vs the running
    image's (`esp_app_get_description()`). **Caveat:** the precompiled Arduino core stamps
    *every* sketch with `project_name = "arduino-lib-builder"`, so the name check only separates
    us from non-Arduino (raw ESP-IDF) images — **the chip-id check is the real guard** against a
    wrong-variant bin. No client-side MD5: the image's appended SHA256, verified by
    `Update.end()`, catches truncation/corruption.
  - **Firmware-update card** in the SETTINGS view of `portal_page.h` (HTML+JS stays there) —
    shows current `FW_VERSION`, file picker + `otaUpload()` `XMLHttpRequest` with
    `upload.onprogress`, and "do not power off / reconnect after reboot" messaging. Upload is
    gated behind a **simple in-page confirm** on the open AP.
  - **On-device feedback:** `otaScreenBegin/Bar/Msg()` draw the "UPDATING — DO NOT POWER OFF"
    screen + byte/percent progress from inside the upload handler (the main loop is blocked for
    the whole transfer — this is the rule-5 exception).
  - **Recovery AP** (the sealed-unit safety net): the 3-way boot gesture `bootHoldGesture()`
    (release = normal / CAL hold / keep holding past CAL = recovery) decides mode *before* any
    sensor or SD init, so a bad driver can't block it. `portalBeginRecovery()` brings up a
    minimal upload-only AP (`RECOVERY_PAGE` + `/api/ota` only) to re-flash a unit whose normal
    firmware boots but misbehaves; `g_recovery` makes `loop()` service just the AP.
- **RTC** (DS3231SN) deferred to the next board revision; firmware already scaffolds
  `nowUnix()` / `g_timeSynced` / `g_timeApprox`.
