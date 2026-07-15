# Viewer app

**Live:** https://gwaxsksjierpzbxugbxj.supabase.co/functions/v1/viewer

The app itself lives in [`../supabase/functions/viewer/`](../supabase/functions/viewer/) —
a single static `index.html` served by the `viewer` edge function so the whole stack is
hosted in Supabase. The function injects `SUPABASE_URL` + the publishable key from its
environment at serve time, so no project keys live in git.

- **Login:** Supabase Auth email/password (raw GoTrue REST, no SDK/CDN — same zero-dependency
  ethos as the firmware portal). Add users via Dashboard → Authentication → Add user
  (auto-confirm). Google OAuth is a later dashboard-config step.
- **Data:** lists `dives` + `allowed_devices` labels via PostgREST, downloads the CSV from
  the private `dives` bucket via `/storage/v1/object/authenticated/…` (no signed URLs
  needed), and renders charts with the portal renderer (`parseCsv`/`decimate`/`miniChart`/
  `drawCharts`) ported from `firmware/src/portal_page.h` — **keep the two in sync** when the
  CSV format changes. Threshold warn/alarm bands are portal-only (the cloud has no
  thresholds); POI markers, raw-channel toggle, and min/max decimation are kept.
- **Deploy:** `supabase functions deploy viewer --use-api`
  (config in `supabase/config.toml` — `verify_jwt = false`, `static_files` bundles the HTML).
