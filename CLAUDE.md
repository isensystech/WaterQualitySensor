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
- App currently ~60% of a 1.98 MB OTA slot.
- The OTA app image is `.pio/build/seeed_xiao_esp32c6/firmware.bin` (the app-partition
  image, **not** a merged/factory bin). This is the file the file-mule OTA flow uploads.

## Hardware

| Part | Bus / pin | Notes |
|---|---|---|
| Seeed XIAO ESP32-C6 | — | MCU, 4 MB flash |
| Adafruit 2.0" ST7789 TFT | SPI | 240×320, portrait |
| Blue Robotics BAR30 (MS5837) | I²C `0x76` | depth/pressure/temp |
| POET electrochemical sensor | I²C `0x1F` | pH (ISFET), ORP, conductivity, salinity, temp |
| SparkFun ADS1015 ADC | I²C `0x48` | optional Turner Cyclops-7F fluorometer (0–5 V) |
| microSD | SPI | logging |
| Twist-actuator momentary button | `D0` | **the only physical input** |

Pin defines live in `shared.h` (`PIN_*`). I²C address defines: `BAR30_ADDR`, plus `0x1F`/`0x48` for POET/ADS.

## Source layout (`src/`)

- **`main.cpp`** — `setup()`/`loop()`, boot sensor self-test, backlight PWM + auto-dim,
  sensor sampling, submerge/logging gate, run-screen rendering (DIVE / DATA pages).
- **`shared.h`** — all cross-file prototypes, includes, `#define`s, globals.
  `FW_VERSION` is defined **canonically here** (currently `0.7.0`).
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
  Keep TX power low (`WIFI_POWER_8_5dBm`), drop the backlight during the write, and never
  run OTA alongside SD activity (it isn't logging on the surface, so this holds naturally).
- **No auto-rollback compiled in** → the stock pioarduino core does not enable
  `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, so a validated-but-buggy image that boots will
  stick. Defences are pre-commit validation (`Update.h` header/size check) + the recovery
  AP gesture. An image that fails to boot *at all* has no software escape — validate hard.

## Current focus / roadmap

- **OTA (in progress)** — "lone-diver file-mule" path: on the surface, a phone/laptop that
  *already downloaded* `firmware.bin` over the internet joins the logger AP and uploads it;
  the logger flashes its inactive slot and reboots. SoftAP↔internet exclusivity means the
  download and the upload are always two separate steps (cache-then-upload). Pieces:
  - **`POST /api/ota`** streaming upload handler in `setup_portal.cpp` — `WebServer`'s
    `on(path, HTTP_POST, onFinish, onUpload)` form + `Update.h`. `Update.begin(UPDATE_SIZE_UNKNOWN)`
    → `write()` per chunk → `end(true)`. **Validate before commit:** `Update` checks the
    ESP32 image magic/size; additionally verify the embedded app-descriptor project name so a
    wrong `.bin` is rejected, not flashed. Refuse if `g_logging`. Reply, then `ESP.restart()`.
  - **Firmware-update card** in the SETTINGS view of `portal_page.h` (HTML+JS stays in that
    file). Shows current `FW_VERSION`; file picker + `XMLHttpRequest` with `upload.onprogress`
    for a progress bar; clear "do not power off / reconnect after reboot" messaging.
  - On-device feedback: draw an "UPDATING — DO NOT POWER OFF" screen + byte/percent progress
    from inside the upload handler (the main loop is blocked during the transfer).
  - **Recovery AP** boot gesture (CAL boot-hold → keep holding past the CAL threshold →
    upload-only AP) as the sealed-unit safety net: minimal server (just `/` + `/api/ota`),
    no sensor init required, for re-flashing a unit whose normal firmware boots but misbehaves.
  - Decisions still open: client-side hash (bundle a tiny MD5 for `Update.setMD5()` vs rely on
    header validation), whether the recovery AP ships in v1 or a follow-up, and whether to gate
    upload behind a confirmation on the open AP.
- **RTC** (DS3231SN) deferred to the next board revision; firmware already scaffolds
  `nowUnix()` / `g_timeSynced` / `g_timeApprox`.
