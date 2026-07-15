# CLAUDE.md — Cloud Stack (Supabase)

Backend for the WQL logger + base station suite. Postgres, Storage, Auth, Edge Functions.
The SHARED CONTRACT for all three stacks — schema changes ripple everywhere.

## Layout
supabase/migrations/  SQL migrations — SOURCE OF TRUTH for the schema
supabase/seed.sql     MAC allowlist seed (placeholder MACs — replace with traced units)
supabase/functions/viewer/  the viewer app (index.html) + edge function that serves it
viewer/               README pointer only — the app lives in supabase/functions/viewer/

Deployed (project ref gwaxsksjierpzbxugbxj, 2026-07-14): migrations 0001+0002 pushed,
seed applied, viewer live at https://gwaxsksjierpzbxugbxj.supabase.co/functions/v1/viewer
Deploy commands: supabase db push | supabase functions deploy viewer --use-api

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
  Mirrors the FULL CSV meta header incl. weather, air_temp_c, notes, and the v0.9.0
  sensor-enable flags (poet_en/bar30_en/cels_en/cyc_en — viewer needs them to tell a
  sensor that was off from one that logged nothing).
- stations(station_id PK, label, lat, lon) — base stations. (not built yet)
- readings(station_id FK, sensor, metric, value, unit, ts, lat, lon, source) — generic env
  suite; wide shape absorbs new sensors without migrations. (not built yet)
FK on dives.device_id IS the allowlist gate (FK validation bypasses RLS -> no MAC-list leak).

Gotchas proven against the live project (don't re-learn these):
- New (2026) projects do NOT auto-grant anon/authenticated on new public tables —
  every migration adding a table MUST add explicit grants or PostgREST 401s.
- Anon upserts are impossible here: ANY ON CONFLICT path (PostgREST merge-/ignore-
  duplicates, storage x-upsert) fails RLS for a role with no SELECT policy. Device
  contract is therefore PLAIN insert/POST; duplicate -> 409 = "already synced"
  (dive files are immutable, so that's still idempotent). Migration 0002 removed the
  anon UPDATE surface entirely. Full request format: docs/DiveSync-To-Do.md Phase 4.

## Build order (this stack)
1. DONE 0001_divesync_init.sql + 0002_no_anon_update.sql — tables, bucket, RLS, grants.
2. DONE seed — placeholder MACs (f412fa000001-03); replace with real traced MACs.
   (deleting a placeholder MAC requires deleting its dives rows first — FK.)
3. (firmware upload leg — other stack — device contract already smoke-tested with a
   synthetic dive: f412fa000001/dive0000.csv, mission "Cloud smoke test"; delete when
   real data flows)
4. 000x_stations_readings.sql — base station tables (when base station work starts).
5. DONE viewer app — supabase/functions/viewer/, reuses the portal chart renderer
   (parseCsv/miniChart/drawCharts ported from firmware/src/portal_page.h — keep in sync).
   Auth users are created by hand: Dashboard -> Authentication -> Add user.

## Don't
- Don't put secrets in git.
- Don't overload dives with env-sensor data — that's what readings is for.
- Don't apply schema through MCP; migrations only.
