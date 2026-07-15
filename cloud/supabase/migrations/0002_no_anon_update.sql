-- 0002_no_anon_update.sql
-- Drop the anon UPDATE surface added in 0001 for upsert support. Tested against the live
-- project (2026-07-14): PostgREST `Prefer: resolution=merge-duplicates` AND `ignore-duplicates`
-- (and storage `x-upsert: true`) all fail 42501 "new row violates row-level security policy"
-- for anon — Postgres applies extra RLS visibility checks on any ON CONFLICT path, and anon
-- deliberately has no SELECT policy. Plain INSERT works, and a duplicate returns a clean
-- 409 (unique_violation / storage Duplicate) the device can treat as "already synced".
-- Dive files are immutable after close, so insert-once + 409-tolerance is fully idempotent.
-- Net effect: device contract is plain POST (no upsert headers), and anon can no longer
-- blindly overwrite existing rows/objects — a strict improvement on the soft-gate model.

revoke update on public.dives from anon;

drop policy if exists dives_update_anon on public.dives;
drop policy if exists dive_upsert_anon  on storage.objects;
