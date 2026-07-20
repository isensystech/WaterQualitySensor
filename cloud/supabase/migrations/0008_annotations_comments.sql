-- 0008_annotations_comments.sql  (DiveViewer Phase 7)
-- Point/region annotations anchored to (dive_id, seq) + a per-dive comment thread, and a
-- private image bucket. Read = any project member; write = annotator/admin on the dive's
-- project and you must be the author. Owner edits/deletes only their own rows.

create table if not exists public.annotations (
  id         uuid primary key default gen_random_uuid(),
  dive_id    uuid not null references public.dives(id) on delete cascade,
  seq        int,                          -- null => whole-dive note (not anchored to a row)
  author_id  uuid not null references public.profiles(id),
  kind       text not null check (kind in ('text','image')),
  body       text,
  image_path text,                         -- object name in the 'annotation-images' bucket
  created_at timestamptz not null default now()
);
create index if not exists annotations_dive_idx on public.annotations (dive_id, seq);

create table if not exists public.comments (
  id         uuid primary key default gen_random_uuid(),
  dive_id    uuid not null references public.dives(id) on delete cascade,
  author_id  uuid not null references public.profiles(id),
  body       text not null,
  created_at timestamptz not null default now()
);
create index if not exists comments_dive_idx on public.comments (dive_id, created_at);

alter table public.annotations enable row level security;
alter table public.comments    enable row level security;
grant select, insert, update, delete on public.annotations to authenticated;
grant select, insert, update, delete on public.comments    to authenticated;

-- annotations
drop policy if exists annotations_read on public.annotations;
create policy annotations_read on public.annotations
  for select to authenticated using (public.can_read_dive(dive_id));
drop policy if exists annotations_insert on public.annotations;
create policy annotations_insert on public.annotations
  for insert to authenticated
  with check (author_id = auth.uid() and public.can_annotate_dive(dive_id));
drop policy if exists annotations_modify_own on public.annotations;
create policy annotations_modify_own on public.annotations
  for update to authenticated using (author_id = auth.uid()) with check (author_id = auth.uid());
drop policy if exists annotations_delete_own on public.annotations;
create policy annotations_delete_own on public.annotations
  for delete to authenticated using (author_id = auth.uid() or public.is_project_admin(
    (select project_id from public.dives d where d.id = dive_id)));

-- comments (any member of the dive's project may comment — DECISION #2)
drop policy if exists comments_read on public.comments;
create policy comments_read on public.comments
  for select to authenticated using (public.can_read_dive(dive_id));
drop policy if exists comments_insert on public.comments;
create policy comments_insert on public.comments
  for insert to authenticated
  with check (author_id = auth.uid() and public.can_read_dive(dive_id));
drop policy if exists comments_modify_own on public.comments;
create policy comments_modify_own on public.comments
  for update to authenticated using (author_id = auth.uid()) with check (author_id = auth.uid());
drop policy if exists comments_delete_own on public.comments;
create policy comments_delete_own on public.comments
  for delete to authenticated using (author_id = auth.uid());

-- ---------------------------------------------------------------- annotation-images bucket
insert into storage.buckets (id, name, public)
values ('annotation-images', 'annotation-images', false)
on conflict (id) do nothing;

-- Cast the leading path segment to uuid WITHOUT throwing — Postgres does not guarantee
-- AND short-circuits in policy quals, so a bare `::uuid` on an object from another bucket
-- (non-uuid name) could error. safe_uuid() returns null instead; can_read_dive(null)=false.
create or replace function public.safe_uuid(t text)
returns uuid language plpgsql immutable set search_path = public as $$
begin
  return t::uuid;
exception when others then
  return null;
end;
$$;
grant execute on function public.safe_uuid(text) to authenticated, anon;

-- Read: any member who can read the dive this image is filed under (path convention:
-- '<dive_id>/<uuid>.<ext>', so the leading path segment is the dive id).
drop policy if exists annimg_read on storage.objects;
create policy annimg_read on storage.objects
  for select to authenticated
  using (bucket_id = 'annotation-images'
         and public.can_read_dive( public.safe_uuid(split_part(name, '/', 1)) ));
-- Insert: annotator/admin on that dive's project.
drop policy if exists annimg_insert on storage.objects;
create policy annimg_insert on storage.objects
  for insert to authenticated
  with check (bucket_id = 'annotation-images'
              and public.can_annotate_dive( public.safe_uuid(split_part(name, '/', 1)) ));
