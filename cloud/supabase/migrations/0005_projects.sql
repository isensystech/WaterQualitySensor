-- 0005_projects.sql  (DiveViewer Phase 5)
-- Human/collaboration layer on top of the device-upload base (0001/0002):
-- profiles, projects, project_members + the SECURITY DEFINER membership helpers that
-- every downstream RLS policy leans on. See dev_docs/DiveViewer-To-Do.md.
--
-- RLS note: a project_members policy that itself SELECTs project_members recurses
-- ("infinite recursion detected in policy"). All membership checks therefore go through
-- SECURITY DEFINER helpers below, which bypass RLS and break the cycle.

-- ---------------------------------------------------------------- profiles
create table if not exists public.profiles (
  id           uuid primary key references auth.users(id) on delete cascade,
  display_name text,
  created_at   timestamptz not null default now()
);

-- Auto-create a profile row when a new auth user signs up (email/password or OAuth).
create or replace function public.handle_new_user()
returns trigger
language plpgsql
security definer
set search_path = public
as $$
begin
  insert into public.profiles (id, display_name)
  values (new.id, coalesce(new.raw_user_meta_data->>'display_name',
                           new.raw_user_meta_data->>'full_name',
                           new.email))
  on conflict (id) do nothing;
  return new;
end;
$$;

drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created
  after insert on auth.users
  for each row execute function public.handle_new_user();

-- ---------------------------------------------------------------- projects
create table if not exists public.projects (
  id         uuid primary key default gen_random_uuid(),
  name       text not null,
  owner_id   uuid not null references public.profiles(id),
  archived   boolean not null default false,
  created_at timestamptz not null default now()
);

create table if not exists public.project_members (
  project_id uuid not null references public.projects(id) on delete cascade,
  user_id    uuid not null references public.profiles(id) on delete cascade,
  role       text not null check (role in ('viewer','annotator','admin')),
  created_at timestamptz not null default now(),
  primary key (project_id, user_id)
);

create index if not exists project_members_user_idx on public.project_members (user_id);

-- ---------------------------------------------------------------- membership helpers
-- SECURITY DEFINER => bypass RLS, so policies can call these without recursing.
create or replace function public.is_project_member(pid uuid)
returns boolean language sql security definer set search_path = public stable as $$
  select exists (
    select 1 from public.project_members m
    where m.project_id = pid and m.user_id = auth.uid()
  );
$$;

create or replace function public.can_annotate(pid uuid)
returns boolean language sql security definer set search_path = public stable as $$
  select exists (
    select 1 from public.project_members m
    where m.project_id = pid and m.user_id = auth.uid()
      and m.role in ('annotator','admin')
  );
$$;

create or replace function public.is_project_admin(pid uuid)
returns boolean language sql security definer set search_path = public stable as $$
  select exists (
    select 1 from public.project_members m
    where m.project_id = pid and m.user_id = auth.uid() and m.role = 'admin'
  )
  or exists (
    select 1 from public.projects p
    where p.id = pid and p.owner_id = auth.uid()
  );
$$;

-- ---------------------------------------------------------------- RLS
alter table public.profiles        enable row level security;
alter table public.projects        enable row level security;
alter table public.project_members enable row level security;

-- New (2026) projects don't auto-grant API roles on new tables (see cloud/CLAUDE.md);
-- without explicit grants PostgREST 401s before RLS is even consulted.
grant select, insert, update on public.profiles        to authenticated;
grant select, insert, update on public.projects        to authenticated;
grant select, insert, update, delete on public.project_members to authenticated;
grant execute on function public.is_project_member(uuid) to authenticated, anon;
grant execute on function public.can_annotate(uuid)      to authenticated, anon;
grant execute on function public.is_project_admin(uuid)  to authenticated, anon;

-- profiles: display names aren't sensitive; any authed user may read them (needed to
-- render annotation/comment authors). You may write only your own row.
drop policy if exists profiles_read_auth on public.profiles;
create policy profiles_read_auth on public.profiles
  for select to authenticated using (true);
drop policy if exists profiles_upsert_self on public.profiles;
create policy profiles_upsert_self on public.profiles
  for insert to authenticated with check (id = auth.uid());
drop policy if exists profiles_update_self on public.profiles;
create policy profiles_update_self on public.profiles
  for update to authenticated using (id = auth.uid()) with check (id = auth.uid());

-- projects: members read; owner/admin update; any authed user may create a project
-- (they must set themselves as owner). Membership is granted separately (bootstrap /
-- future admin UI) — creating a project does not auto-insert a members row here.
drop policy if exists projects_read_member on public.projects;
create policy projects_read_member on public.projects
  for select to authenticated using (public.is_project_member(id) or owner_id = auth.uid());
drop policy if exists projects_insert_owner on public.projects;
create policy projects_insert_owner on public.projects
  for insert to authenticated with check (owner_id = auth.uid());
drop policy if exists projects_update_admin on public.projects;
create policy projects_update_admin on public.projects
  for update to authenticated using (public.is_project_admin(id)) with check (public.is_project_admin(id));

-- project_members: you can see the membership rows of projects you belong to; admins/owner
-- manage membership. Self-reference is safe here because is_project_* are SECURITY DEFINER.
drop policy if exists members_read on public.project_members;
create policy members_read on public.project_members
  for select to authenticated using (user_id = auth.uid() or public.is_project_member(project_id));
drop policy if exists members_write_admin on public.project_members;
create policy members_write_admin on public.project_members
  for all to authenticated
  using (public.is_project_admin(project_id))
  with check (public.is_project_admin(project_id));
