<img width="1920" height="1080" alt="Frame 1" src="https://github.com/user-attachments/assets/256ee78b-27e7-47b8-af31-f085864ad601" />

# Dive WaterQuality Logger

A **dive-deployable water-quality logger** — a watertight, sealed instrument that divers carry
underwater to sample and record water conditions in real time. The tube never opens in the field,
so there is **no USB access**: everything runs from a single push button, an on-screen UI, and a
wireless setup portal. Calibration, mission setup, data offload, and even firmware updates all
happen without breaking the seal.

The logger measures **depth, pressure, and temperature** alongside a full electrochemical suite —
**pH, ORP, conductivity, and salinity** — with an optional fluorometer channel and high-accuracy
temperature sensor. Readings show live on a 2.0" color display and log to microSD as CSV, so a
dive's data comes home on the card. Every I²C sensor is individually enable-able and auto-detected
at boot, and each channel blanks independently when it is absent or still needs calibrating.

---

## Hardware

| Part | Bus / pin | Notes |
|---|---|---|
| Seeed XIAO ESP32-C6 | — | MCU, 4 MB flash, 320 KB RAM, RISC-V |
| Adafruit 2.0" ST7789 TFT | SPI | 240×320, portrait |
| Blue Robotics BAR30 (MS5837) | I²C `0x76` | depth / pressure / temp |
| POET electrochemical sensor | I²C `0x1F` | pH (ISFET), ORP, conductivity, salinity, temp |
| SparkFun ADS1015 ADC | I²C `0x48` | optional Turner Cyclops-7F fluorometer (0–5 V) |
| Blue Robotics Celsius (TSYS01) | I²C `0x77` | optional high-accuracy temperature |
| microSD | SPI | logging |
| Momentary push button | `D0` | **the only physical input** |

All four I²C sensors are individually enable-able and auto-detected at boot. A disabled or
absent sensor blanks independently (`--` on screen, empty CSV column) rather than erroring.

## Features

- **Single-button UI** — every gesture works with the one `D0` push button (no touch).
- **Submerge-gated logging** — starts on POET conductivity or BAR30 depth (>0.5 m), whichever
  is fitted; DIVE / DATA run screens; logs `dive*.csv` to microSD. A 3-second button hold
  forces logging on/off for bench tests without water (footer shows `LOG*` while forced).
- **On-device calibration** — pH (3-pt), EC (1-pt), ORP (1-pt), Cyclops (2-pt), with a
  `callog.csv` audit trail.
- **Captive-portal setup** — SoftAP web app for settings, mission config, thresholds, and
  calibration without opening the tube.
- **Wireless OTA updates** — "file-mule" flow: a phone/laptop that already downloaded the
  firmware joins the logger's AP and uploads it to the inactive OTA slot.
- **Recovery AP** — a boot gesture brings up an upload-only AP to re-flash a misbehaving unit
  without opening the seal.

## Repository layout

```
src/
  main.cpp          setup()/loop(), boot self-test, sampling, run-screen rendering
  shared.h          cross-file prototypes, includes, #defines, globals (FW_VERSION lives here)
  calibration.cpp   on-device cal flows + salinity/PSU math
  setup_portal.cpp  SoftAP captive portal, /api/* REST, state.json + cal.json persistence
  portal_page.h     the single-page web app (HTML/CSS/JS) as one PROGMEM string
partitions_ota.csv  locked dual-slot OTA partition table (no SPIFFS)
platformio.ini      PlatformIO project config (pioarduino platform fork)
CLAUDE.md           detailed engineering notes, hard rules, and roadmap
```

**SD card files at runtime:** `state.json` (settings/mission/time/thresholds),
`cal.json` (calibration), `dive*.csv` (logs), `callog.csv` (calibration audit log).

## Toolchain

This is a [PlatformIO](https://platformio.org/) project targeting the
`seeed_xiao_esp32c6` board.

> ⚠️ It uses the community **[pioarduino](https://github.com/pioarduino/platform-espressif32)**
> platform fork (pinned in `platformio.ini`). The official `platformio/espressif32` platform
> does **not** support the Arduino framework on the ESP32-C6 — do not switch to it.

## Build / flash / monitor

```bash
pio run                  # build
pio run -t upload        # flash over USB (close the serial monitor first)
pio device monitor       # serial console, 115200 baud, over the C6 native USB
pio run -t clean         # clean
```

**Common gotchas**

- *Upload "Resource busy" / can't open port* → the serial monitor is holding the port. Close
  it and re-upload (last resort: hold BOOT, tap RESET to force download mode).
- *Blank serial monitor* → set `ARDUINO_USB_CDC_ON_BOOT=0` in `platformio.ini`.

## Releases

Release binaries are **not committed** — they are published as GitHub Release assets, with the
commit tagged `v<VER>`. Each release ships two products built from `.pio/build/seeed_xiao_esp32c6/`:

| Asset | Source | Use |
|---|---|---|
| `firmware-v<VER>.bin` | `firmware.bin` | **OTA** app image — the file uploaded wirelessly |
| `WaterQuality-<VER>-flash-at-0x0.bin` | `firmware.factory.bin` | Full USB flash / recovery seed |

```bash
gh release create v<VER> \
  builds/firmware-v<VER>.bin \
  builds/WaterQuality-<VER>-flash-at-0x0.bin
```

> ⚠️ **USB flashing the factory bin uses offset `0x0`, not `0x1000`.** The ESP32-**C6**
> bootloader lives at `0x0`; esptool-js defaults to `0x1000` (correct for classic ESP32, wrong
> here) which yields a black, non-booting screen. Set address `0`, Erase Flash, re-flash. The
> OTA app bin (`firmware-v*.bin`) is unaffected.

## Contributing

This is proprietary client firmware. See **[CLAUDE.md](CLAUDE.md)** for the engineering notes,
the **hard rules** that must not be broken (locked partition table, verbatim credits block,
single physical input, non-blocking run path, independent sensor blanking), and the current
roadmap. Read those rules before changing anything.

## License

Proprietary — Copyright © 2026 iSENSYS / Fieldwerx. All rights reserved. See [LICENSE](LICENSE).

## Credits

Developed by **iSENSYS** for **Fieldwerx**. © 2026.
