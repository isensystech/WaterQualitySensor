# WQL Dive Viewer

DiveSync cloud viewer — a Vite + React + TS SPA for viewing/annotating/comparing dive logs.
Reads the dives the loggers upload (device path, migrations 0001/0002) through Supabase Auth
+ RLS. Chart renderer is ported from `firmware/src/portal_page.h` (Hard Rule 6 — keep in sync).

This is **Phase 5 (MVP)** of `firmware/dev_docs/DiveViewer-To-Do.md`: login, calendar,
dive list, and the multi-metric dive-graph screen with per-metric scaling, metric-visibility
toggles, and a threshold-marker toggle. Map / annotations / compare / NOAA are later phases.

## Local dev
```bash
cd cloud/apps/viewer
cp .env.example .env.local          # values already point at the live project (both public)
npm install
npm run dev                         # http://localhost:5173
npm run build                       # tsc --noEmit && vite build  -> dist/
```

## Go-live checklist (needs your Supabase + Cloudflare credentials)

These steps could **not** be run from the build session (no interactive OAuth / DB password).

1. **Push the schema** (from `cloud/`, migrations only — Hard Rule 1):
   ```bash
   supabase db push          # applies 0005_projects … 0009_thresholds
   ```
2. **Regenerate types** (replaces the hand-authored stub in `src/database.types.ts`):
   ```bash
   supabase gen types typescript --linked > apps/viewer/src/database.types.ts
   ```
3. **Deploy the parser** and (optionally) set its shared secret:
   ```bash
   supabase functions deploy parse-dive
   supabase secrets set PARSE_DIVE_SECRET=<random>     # optional; sent as x-parse-secret
   ```
4. **Create your login**: Dashboard → Authentication → Add user (email/pass), or enable Google
   OAuth. For Google + the deployed site, add the Pages URL to Auth → URL Configuration
   (redirect allowlist) and to the Google OAuth client.
5. **Bootstrap membership + backfill dives** (edit the email inside first):
   ```bash
   supabase db execute --file supabase/scripts/bootstrap_viewer.sql
   ```
6. **Backfill `samples`** for existing dives (idempotent; needed for Phase 8 compare, not for
   the Phase 5 chart which reads the blob directly):
   ```bash
   curl -X POST "$SUPABASE_URL/functions/v1/parse-dive" \
     -H "content-type: application/json" \
     -H "x-parse-secret: <secret-if-set>" \
     -d '{"all":true}'
   ```
7. **Deploy to Cloudflare Pages** (account `tech@isensys.com`, DECISION #4):
   - Connect the GitHub repo; **root directory** = `cloud/apps/viewer`.
   - Build command `npm run build`; output directory `dist`.
   - Build env vars: `VITE_SUPABASE_URL`, `VITE_SUPABASE_ANON_KEY` (the publishable key).
   - SPA routing is handled by `public/_redirects`.

## Smoke test (Phase 5 acceptance)
Log in → the calendar marks days with dives (badge = count) → pick a dive → the chart renders
with correct metrics, per-metric scaling, and (once a project sets `thresholds`) warn/alarm
bands. A non-member sees no dives (RLS).

## Security notes
- Only the **publishable/anon** key ships in the bundle (Hard Rule 3). The secret key lives
  only in Edge Functions / base station — never here, the browser, or git.
- All viewer reads are RLS-scoped to `project_members`; the device anon-insert path is
  untouched (Hard Rule 4).
