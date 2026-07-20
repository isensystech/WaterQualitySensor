-- bootstrap_viewer.sql — one-time viewer bootstrap (NOT a reset seed).
-- Run AFTER `supabase db push` (0005–0009) AND after your Supabase Auth user exists
-- (Dashboard -> Authentication -> Add user, or sign up once through the viewer).
--
-- It creates a demo project, makes you its admin, and assigns every currently-unassigned
-- dive to that project so they become visible under the new project-scoped RLS. Idempotent.
--
-- Run it with either:
--   supabase db execute --file supabase/scripts/bootstrap_viewer.sql     (edit the email first)
--   or paste into the Supabase SQL editor.

do $$
declare
  v_email text := 'you@example.com';   -- <<< EDIT: the Supabase Auth email to grant access to
  v_uid   uuid;
  v_pid   uuid;
begin
  select id into v_uid from auth.users where email = v_email;
  if v_uid is null then
    raise exception 'No auth user with email %. Create it (Dashboard -> Authentication -> Add user) then re-run.', v_email;
  end if;

  insert into public.profiles (id, display_name)
  values (v_uid, v_email)
  on conflict (id) do nothing;

  select id into v_pid from public.projects where name = 'Demo Project' and owner_id = v_uid;
  if v_pid is null then
    insert into public.projects (name, owner_id) values ('Demo Project', v_uid) returning id into v_pid;
  end if;

  insert into public.project_members (project_id, user_id, role)
  values (v_pid, v_uid, 'admin')
  on conflict (project_id, user_id) do update set role = 'admin';

  update public.dives set project_id = v_pid where project_id is null;

  raise notice 'Bootstrap OK: % -> project % (% dives now in project)',
    v_email, v_pid, (select count(*) from public.dives where project_id = v_pid);
end $$;
