# WaterQuality Logger — firmware update (Jessica & Chandler)

You have two files. **They are not interchangeable:**

| File | Use it for | How |
|---|---|---|
| `WaterQuality-0.8.1-flash-at-0x0.bin` | **One time only** — the USB flash below | esptool-js, **offset `0x0`** |
| `firmware-v0.8.1.bin` | **Every future update** — wireless | Logger's web page (Part 3) |

---

## Part 1 — One-time USB flash (open the tube, last time)

You only do this once, to install OTA. After this the tube stays sealed.

- [ ] Slide the stack out. Plug the **XIAO ESP32-C6** into a computer with a USB-C **data** cable.
- [ ] In **Chrome or Edge**, open **https://espressif.github.io/esptool-js/**
- [ ] Click **Connect**, pick the XIAO's serial port.
      - If it won't connect or won't start programming: hold **BOOT**, tap **RESET**, release **BOOT**, then Connect again.
- [ ] In the file row: Flash Address = **`0x0`**, file = **`WaterQuality-0.8.1-flash-at-0x0.bin`**
- [ ] Click **Program**. Wait for it to finish (100% / "Done").
- [ ] Press **RESET** (or unplug/replug).

## Part 2 — Verify before you seal it

- [ ] The screen boots normally.
- [ ] Join Wi-Fi **`WaterQuality-Logger`**, open **`192.168.4.1`** in a browser.
- [ ] Top of the page reads **firmware 0.8.1**. ✅
- [ ] Reassemble / seal the tube. **This is the last time it needs opening.**

---

## Part 3 — Every update from now on (wireless, no tools, no opening)

When we send a new `firmware-vX.Y.Z.bin`:

1. **On internet** (cellular / hotel Wi-Fi), download the `.bin` we send. Note where it saved.
2. Join Wi-Fi **`WaterQuality-Logger`**, open **`192.168.4.1`**.
3. **SETTINGS → Firmware update →** choose the `.bin` → confirm.
   - iPhone: tap **Browse → Files → Downloads** to find the `.bin`.
4. Watch the bar. The screen says **UPDATING — DO NOT POWER OFF**. It reboots itself.
5. Rejoin the Wi-Fi, reopen the page, check the version went up. Done.

The logger's own **HELP → "Updating the firmware"** topic has these same steps.

### If anything goes wrong
- A wrong/corrupt/interrupted upload is **harmless** — the logger keeps its old firmware. Just retry.
- Only upload the **app `.bin`** we send (e.g. `firmware-v0.8.1.bin`), not the `flash-at-0x0` file — the page will reject that one.
- **Recovery mode (safety net):** if an update ever leaves a unit misbehaving, hold the **twist** while powering on and **keep holding past the CALIBRATE prompt** until the screen says **RECOVERY**. That's an upload-only Wi-Fi — join `WaterQuality-Logger`, open `192.168.4.1`, upload a good `.bin`. Power-cycle to leave.
