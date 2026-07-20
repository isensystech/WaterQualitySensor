-- 0006_sites_dives.sql  (DiveViewer Phase 5/6)
-- Sites (global gazetteer) + attach dives to a project/site, and TIGHTEN the dive read
-- path from "any authed user" (0001) to project-membership scoped.
--
-- Reconciliation with the LIVE schema (the To-Do predates 0001): dives already has
-- lat/lon and utc_start from 0001, so we do NOT re-add them. project_id is left
-- NULLABLE on purpose: the device anon-insert path (Hard Rule 4) knows nothing about
-- projects, so dives arrive unassigned and are assigned later (bootstrap script /
-- Phase 10 move-to-project). Unassigned dives are simply invisible to members via RLS.

-- ---------------------------------------------------------------- sites
create table if not exists public.sites (
  id         uuid primary key default gen_random_uuid(),
  name       text not null,
  lat        double precision,
  lon        double precision,
  radius_m   int not null default 50,
  created_at timestamptz not null default now()
);

-- ---------------------------------------------------------------- dives additions
alter table public.dives add column if not exists project_id uuid references public.projects(id);
alter table public.dives add column if not exists site_id    uuid references public.sites(id);
alter table public.dives add column if not exists archived   boolean not null default false;
alter table public.dives add column if not exists label      text;
alter table public.dives add column if not exists started_at timestamptz;   -- parse: min(sample ts) or utc_start
alter table public.dives add column if not exists ended_at   timestamptz;   -- parse: max(sample ts)

create index if not exists dives_project_idx on public.dives (project_id);
create index if not exists dives_site_idx    on public.dives (site_id);

-- ---------------------------------------------------------------- dive read helpers
-- Keyed by dive id and by storage object name; SECURITY DEFINER so storage/samples/
-- annotation policies can reuse the same membership logic without RLS recursion.
create or replace function public.can_read_dive(did uuid)
returns boolean language sql security definer set search_path = public stable as $$
  select exists (
    select 1 from public.dives d
    join public.project_members m on m.project_id = d.project_id
    where d.id = did and m.user_id = auth.uid()
  );
$$;

create or replace function public.can_read_dive_object(obj text)
returns boolean language sql security definer set search_path = public stable as $$
  select exists (
    select 1 from public.dives d
    join public.project_members m on m.project_id = d.project_id
    where d.storage_path = obj and m.user_id = auth.uid()
  );
$$;

create or replace function public.can_annotate_dive(did uuid)
returns boolean language sql security definer set search_path = public stable as $$
  select exists (
    select 1 from public.dives d
    join public.project_members m on m.project_id = d.project_id
    where d.id = did and m.user_id = auth.uid() and m.role in ('annotator','admin')
  );
$$;

grant execute on function public.can_read_dive(uuid)          to authenticated, anon;
grant execute on function public.can_read_dive_object(text)   to authenticated, anon;
grant execute on function public.can_annotate_dive(uuid)      to authenticated, anon;

-- ---------------------------------------------------------------- sites RLS
alter table public.sites enable row level security;
grant select, insert, update on public.sites to authenticated;

drop policy if exists sites_read_auth on public.sites;
create policy sites_read_auth on public.sites
  for select to authenticated using (true);            -- gazetteer is shared across projects
drop policy if exists sites_insert_auth on public.sites;
create policy sites_insert_auth on public.sites
  for insert to authenticated with check (true);
drop policy if exists sites_update_auth on public.sites;
create policy sites_update_auth on public.sites
  for update to authenticated using (true) with check (true);

-- ---------------------------------------------------------------- dives read path (TIGHTEN)
-- 0001 granted authenticated a blanket read (using true). Replace with project scope.
-- The anon INSERT policy/grant from 0001 is left UNTOUCHED (Hard Rule 4).
drop policy if exists dives_read_auth on public.dives;
create policy dives_read_member on public.dives
  for select to authenticated
  using (project_id is not null and public.is_project_member(project_id));

-- Members with admin/annotator rights on a dive's project may update its metadata
-- (label, site_id, archived, project reassignment is Phase 10). Anon still cannot update.
drop policy if exists dives_update_member on public.dives;
create policy dives_update_member on public.dives
  for update to authenticated
  using (project_id is not null and public.can_annotate(project_id))
  with check (project_id is null or public.can_annotate(project_id));
grant update on public.dives to authenticated;

-- ---------------------------------------------------------------- storage read path (TIGHTEN)
-- 0001 let any authed user read the whole 'dives' bucket. Scope to project membership.
drop policy if exists dive_read_auth on storage.objects;
create policy dive_read_member on storage.objects
  for select to authenticated
  using (bucket_id = 'dives' and public.can_read_dive_object(name));
