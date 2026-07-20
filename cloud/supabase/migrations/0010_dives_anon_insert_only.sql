-- 0010_dives_anon_insert_only.sql  (DiveViewer Phase 5 — grant-layer hardening)
-- Scope: public.dives ONLY. Make anon INSERT-only on the one anon-writable table, so the
-- GRANT layer agrees with the RLS layer (belt-and-suspenders / defense-in-depth, matching the
-- rest of the project). Background: this project's default privileges hand anon full CRUD on
-- every new public table (verified live via pg_default_acl 2026-07-20), and RLS is what keeps
-- anon out. On `dives`, anon genuinely needs only INSERT (the device upload leg), so we drop
-- the rest at the privilege layer too: an anon SELECT now fails closed with 42501 (401 via
-- PostgREST) instead of returning an RLS-emptied 200 [].
--
-- Device path is UNAFFECTED (Hard Rule 4): anon keeps INSERT; the dives.device_id ->
-- allowed_devices FK check is owner-privileged (doesn't need anon SELECT on the parent); the
-- 'dives' storage bucket policies are separate and untouched.
--
-- NOTE: this is deliberately NARROW. samples/projects/annotations/comments/thresholds/sites
-- keep the broad-grant + RLS-only posture for now; a project-wide grant-hardening pass is a
-- separate, deliberate change later — not part of this Phase 5 fix.

revoke all    on public.dives from anon;   -- clears the default-privilege SELECT/DELETE/etc.
grant  insert on public.dives to   anon;   -- keep only what the device upload leg needs
