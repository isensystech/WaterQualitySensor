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
  utc_start     timestamptz,
  time_source   text,
  cal_ph        boolean,
  cal_ec        boolean,
  cal_orp       boolean,
  cal_cyc       boolean,
  cyclops_units text,
  row_count     int,
  uploaded_at   timestamptz not null default now(),
  unique (device_id, filename)
);

create index if not exists dives_device_idx on public.dives (device_id);
create index if not exists dives_utc_idx    on public.dives (utc_start);

alter table public.dives           enable row level security;
alter table public.allowed_devices enable row level security;

drop policy if exists dives_insert_anon on public.dives;
create policy dives_insert_anon on public.dives
  for insert to anon with check (true);        -- FK rejects unknown MACs

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

drop policy if exists dive_read_auth on storage.objects;
create policy dive_read_auth on storage.objects
  for select to authenticated using (bucket_id = 'dives');
