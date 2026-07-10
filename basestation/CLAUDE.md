# CLAUDE.md — Base Station Stack (Raspberry Pi 5)

Field aggregator + environmental station + cloud gateway. See /docs/BaseStation-Arch.md.
PRE-STAGING — build starts after cloud + firmware upload leg work.

## Topology (locked)
- Pi-as-AP: wlan0 = hostapd + dnsmasq, the AP WQL loggers join.
- Separate uplink: eth0 / USB-WiFi wlan1 / cellular wwan0 — NOT the AP radio.
- Route-agnostic offload: watches for ANY default route; buffers to Timescale when offline,
  drains when a route appears. Never required to be always-online.

## Stack (locked)
drivers -> Mosquitto (MQTT) -> writer -> TimescaleDB -> local API -> kiosk UI (Chromium)
                                                      -> offload svc -> Supabase (secret key)
- Drivers: one process per protocol -> normalize -> publish to MQTT
  (modbus poller, gpio rain counter, gpsd, lora rx, logger HTTP endpoint, apera bridge).
- Timescale: hypertables + continuous aggregates. Mirrors Supabase readings schema.
- Kiosk UI: custom SPA reusing firmware portal chart renderer; local API (FastAPI/Node)
  serves history (REST) + live tiles (websocket). CONFIRM: clone adjacent project's SPA.

## Sensor interfaces
- RS485/Modbus (locked) for wired env sensors. Draft addr map: 0x01 soil, 0x02 air,
  0x03 noise, 0x04 creek. HARD RULE: buy Modbus/SDI-12/digital variants, never analog (no ADC).
- LoRa US915 point-to-point (locked) for the buoy.
- GPIO pulse counting for the tipping-bucket rain gauge.
- Apera PC60: dumb LCD, must RE. Ship a manual-entry card (readings.source='manual') as the
  guaranteed path; logic-analyze the MCU pins before committing to full RE.

## Cloud offload
Pi is a TRUSTED SERVER -> holds the secret key, writes stations/readings directly. No
publishable-key/allowlist dance (that's the sealed-logger path only). Secret in .env, never in git.

## Open (pre-staging)
Uplink radio BOM · confirm UI stack · source noise/creek as Modbus · Apera RE-vs-manual.
