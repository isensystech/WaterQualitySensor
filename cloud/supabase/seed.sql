-- seed.sql — pre-seed the MAC allowlist with our known/traced units.
-- Base MAC, lowercase hex, no colons. REPLACE with real MACs before testing uploads.

insert into public.allowed_devices (mac, label) values
  ('f412fa000001', 'Unit 01'),
  ('f412fa000002', 'Unit 02'),
  ('f412fa000003', 'Unit 03')
on conflict (mac) do nothing;

-- Find a unit's base MAC in firmware: WiFi.macAddress() / esp_read_mac(...).
-- Normalize to lowercase, strip colons, before inserting here AND before the device sends it.
