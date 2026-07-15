-- 0001_divesync_init.sql
-- DiveSync cloud base: MAC allowlist + dives table + storage bucket + RLS.
-- Apply with:  supabase db push
-- See docs/DiveSync-To-Do.md Phase 4 for rationale.

create table if not exists public.allowed_devices (
  mac         text primary key,      -- base MAC, lowercase hex, no colons: 'f412fa123456'
  label       text,
  secret_hash text,                  -- nullable now; sha256(per-device secret) for hardening
  added_at    timestamptz not null default now()
);

create table if not exists public.dives (
  id            uuid primary key default gen_random_uuid(),
  device_id     text not null references public.allowed_devices(mac),  -- FK = allowlist gate
  filename      text not null,
  storage_path  text not null,
  cast_num      int,
  mission       text,
  operator      text,
  site          text,
  water_type    text,
  lat           double precision,
  lon           double precision,
  weather       text,                -- CSV '# weather:' (free text)
  air_temp_c    double precision,    -- CSV '# weather: ... air_C:'
  notes         text,                -- CSV '# notes:'
  utc_start     timestamptz,
  time_source   text,                -- PHONE | PHONE_APPROX | UNSYNCED
  cal_ph        boolean,
  cal_ec        boolean,
  cal_orp       boolean,
  cal_cyc       boolean,
  poet_en       boolean,             -- CSV '# sensors:' line (v0.9.0+) — which sensors were
  bar30_en      boolean,             -- enabled; a disabled sensor logs blank columns, so the
  cels_en       boolean,             -- viewer needs these to tell "off" from "no data"
  cyc_en        boolean,
  cyclops_units text,
  row_count     int,
  uploaded_at   timestamptz not null default now(),
  unique (device_id, filename)
);

create index if not exists dives_device_idx on public.dives (device_id);
create index if not exists dives_utc_idx    on public.dives (utc_start);

alter table public.dives           enable row level security;
alter table public.allowed_devices enable row level security;

-- New (2026) projects do NOT auto-grant API roles on new public tables — without these
-- explicit grants PostgREST returns 401/42501 before RLS is even consulted.
grant insert, update on public.dives           to anon;   -- update: upsert path only, see below
grant select         on public.dives           to authenticated;
grant select         on public.allowed_devices to authenticated;

drop policy if exists dives_insert_anon on public.dives;
create policy dives_insert_anon on public.dives
  for insert to anon with check (true);        -- FK rejects unknown MACs

-- Devices upsert with Prefer: resolution=merge-duplicates so a re-upload after a lost
-- sync manifest is idempotent; ON CONFLICT DO UPDATE needs an anon update policy.
-- Blind overwrite by anon is within the soft-gate trust model (no select policy — anon
-- still can't read anything, and the FK still pins device_id to the allowlist).
drop policy if exists dives_update_anon on public.dives;
create policy dives_update_anon on public.dives
  for update to anon using (true) with check (true);

drop policy if exists dives_read_auth on public.dives;
create policy dives_read_auth on public.dives
  for select to authenticated using (true);

drop policy if exists devices_read_auth on public.allowed_devices;
create policy devices_read_auth on public.allowed_devices
  for select to authenticated using (true);

insert into storage.buckets (id, name, public)
values ('dives', 'dives', false)
on conflict (id) do nothing;

drop policy if exists dive_upload_anon on storage.objects;
create policy dive_upload_anon on storage.objects
  for insert to anon with check (bucket_id = 'dives');

-- Device uploads use x-upsert: true (idempotent re-upload); storage upsert needs update too.
drop policy if exists dive_upsert_anon on storage.objects;
create policy dive_upsert_anon on storage.objects
  for update to anon using (bucket_id = 'dives') with check (bucket_id = 'dives');

drop policy if exists dive_read_auth on storage.objects;
create policy dive_read_auth on storage.objects
  for select to authenticated using (bucket_id = 'dives');
