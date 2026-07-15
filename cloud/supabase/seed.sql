-- seed.sql — pre-seed the MAC allowlist with our known/traced units.
-- Base MAC, lowercase hex, no colons. Applied to the live project 2026-07-15.

insert into public.allowed_devices (mac, label) values
  ('a21eb1610736', 'Unit 0736'),
  ('58e6c512d60d', 'Unit d60d'),
  ('58e6c512f885', 'Unit f885'),
  ('f412fa000001', 'Sample data (synthetic)')  -- keeps demo dive0000.csv viewable until real dives flow
on conflict (mac) do nothing;

-- Find a unit's base MAC in firmware: WiFi.macAddress() / esp_read_mac(...).
-- Normalize to lowercase, strip separators, before inserting here AND before the device sends it.
-- CAUTION: a21eb1610736 has the locally-administered bit set (a2:...) — that is usually a
-- randomized/softAP MAC, not an Espressif base MAC (cf. 58:e6:c5 = Espressif OUI). Verify it
-- against the unit's WiFi.macAddress() before relying on it.
