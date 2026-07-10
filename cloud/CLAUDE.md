# CLAUDE.md — Cloud Stack (Supabase)

Backend for the WQL logger + base station suite. Postgres, Storage, Auth, Edge Functions.
The SHARED CONTRACT for all three stacks — schema changes ripple everywhere.

## Layout
supabase/migrations/  SQL migrations — SOURCE OF TRUTH for the schema
supabase/seed.sql     MAC allowlist seed (known units)
viewer/               static viewer app (Supabase Auth login, lists dives, renders charts)
functions/            edge functions (none at MVP; per-device-secret proxy = hardening path)

## Working with Supabase
- Schema = CLI migrations: supabase migration new, edit SQL, supabase db push.
  Never hand-edit prod schema in the dashboard — it drifts from the repo.
- Supabase MCP may be connected READ-ONLY (?read_only=true) for inspection. It bypasses
  RLS (service-role level) — do NOT do schema writes through it; migrations only.

## Auth model — two SEPARATE problems
1. Device -> cloud (logger upload): publishable key (sb_publishable_...) on the apikey
   header ONLY (Bearer rejects it). Gate = MAC allowlist via FK constraint. Soft gate
   (MACs spoofable) — fine for trusted fleet. Hardening = per-device secret + edge function.
2. Human -> cloud (viewer): Supabase Auth (email/password or Google OAuth).
Secret key (sb_secret_...) = base station + edge functions only. Never on a device, never in git.

## Schema summary
- allowed_devices(mac PK, label, secret_hash, added_at) — known units, pre-seeded.
- dives(... device_id FK->allowed_devices, unique(device_id,filename) ...) — logger dives.
- stations(station_id PK, label, lat, lon) — base stations.
- readings(station_id FK, sensor, metric, value, unit, ts, lat, lon, source) — generic env
  suite; wide shape absorbs new sensors without migrations.
FK on dives.device_id IS the allowlist gate (FK validation bypasses RLS -> no MAC-list leak).

## Build order (this stack)
1. 0001_divesync_init.sql — allowed_devices, dives, storage bucket, RLS.  <- first
2. Seed known MACs.
3. (firmware upload leg — other stack — tests this end to end)
4. 000x_stations_readings.sql — base station tables (when base station work starts).
5. Viewer app — reuses firmware portal chart renderer (parseCsv/miniChart/drawCharts).

## Don't
- Don't put secrets in git.
- Don't overload dives with env-sensor data — that's what readings is for.
- Don't apply schema through MCP; migrations only.
