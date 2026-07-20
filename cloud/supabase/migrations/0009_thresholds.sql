-- 0009_thresholds.sql  (DiveViewer Phase 8)
-- Warn/crit bands per metric that drive the chart's threshold markers. A row with
-- project_id = null is a GLOBAL default; a project row overrides it for that project.
-- metric matches the firmware/portal band keys: depth,temp,ph,ec,sal,orp,cyc.
-- Field mapping to the ported bandSvg(): warn_lo/warn_hi -> wlo/whi (amber),
-- crit_lo/crit_hi -> alo/ahi (red). A null bound disables that edge.

create table if not exists public.thresholds (
  id         uuid primary key default gen_random_uuid(),
  project_id uuid references public.projects(id) on delete cascade,   -- null = global default
  metric     text not null,
  warn_lo    real,
  warn_hi    real,
  crit_lo    real,
  crit_hi    real,
  created_at timestamptz not null default now()
);
-- one band per (project, metric); the null-project rows collide under a partial unique index
create unique index if not exists thresholds_proj_metric_idx
  on public.thresholds (coalesce(project_id, '00000000-0000-0000-0000-000000000000'::uuid), metric);

alter table public.thresholds enable row level security;
grant select, insert, update, delete on public.thresholds to authenticated;

-- Read: global rows are visible to everyone; project rows to that project's members.
drop policy if exists thresholds_read on public.thresholds;
create policy thresholds_read on public.thresholds
  for select to authenticated
  using (project_id is null or public.is_project_member(project_id));
-- Write: project admins manage their project's rows. Global (null) rows are seeded by
-- migration / service role only — no anon or authenticated write path to them.
drop policy if exists thresholds_write_admin on public.thresholds;
create policy thresholds_write_admin on public.thresholds
  for all to authenticated
  using (project_id is not null and public.is_project_admin(project_id))
  with check (project_id is not null and public.is_project_admin(project_id));

-- Global defaults are intentionally NOT seeded here: warn/crit limits are science- and
-- deployment-specific (fresh vs salt, target organism, regulatory context). Ship empty so
-- the UI shows no bands until a project sets them. Example, to be tuned per project:
--   insert into public.thresholds (project_id, metric, warn_lo, warn_hi, crit_lo, crit_hi)
--   values (null, 'ph', 6.5, 8.5, 6.0, 9.0);
