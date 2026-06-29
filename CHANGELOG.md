# Changelog

All notable changes to the Dive WaterQuality Logger firmware are documented here.
Versions track `FW_VERSION` in [`src/shared.h`](src/shared.h) — the single source of truth.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

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

[0.9.2]: https://github.com/
[0.9.1]: https://github.com/
[0.9.0]: https://github.com/
[0.8.1]: https://github.com/
[0.8.0]: https://github.com/
