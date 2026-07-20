# DiveViewer-To-Do.md — DiveSync Cloud Viewer

Frontend + backend for viewing/annotating/comparing dive logs.
Continues from `DiveSync-To-Do.md` **Phase 4** (device→cloud upload — DONE; logger is
writing raw CSV + `dives` metadata into Supabase, gated by the `allowed_devices` MAC FK).

Stack lives in the cloud monorepo: `cloud/supabase/migrations/*.sql` (schema),
`cloud/apps/viewer/` (Vite + React + TS SPA). Firmware is a separate stack — untouched here.

---

## DECISIONS — LOCKED (2026-07-20)

| # | Question | **Locked** |
|---|---|---|
| 1 | Dive data model | **Hybrid** — raw CSV blob = immutable provenance; `parse-dive` explodes into `samples` for SQL compare + `(dive_id,seq)` annotation anchors |
| 2 | Sharing model | **Per-project membership** via `project_members`; public share links deferred |
| 3 | NOAA normals source | **WOA climatology** (monthly T/salinity normals gridded by lat/lon/depth); CO-OPS station obs added later where a station is near a site |
| 4 | Frontend host | **Cloudflare Pages** (account: `tech@isensys.com`); Worker reserved for the NOAA fetch/cache proxy |

---

## HARD RULES — do not break

1. **Schema = CLI migrations only.** All DDL in `cloud/supabase/migrations/NNNN_*.sql`,
   applied with `supabase db push`. **Never apply schema through the MCP server.**
2. **Supabase MCP is read-only.** Add it as `http` in `.mcp.json` with `?read_only=true`.
   It connects at service-role level and **bypasses RLS** — use it for inspect / query /
   `gen types` only, never prod writes. Do schema work on a Supabase **branch**, not prod.
3. **Keys.** Viewer bundle ships the **publishable/anon key** only, with Auth + RLS doing
   the gating. The **secret key never touches the viewer, the browser, or git** — it lives
   only in Edge Functions / the base station. `.env*` is gitignored; keys injected as
   Cloudflare Pages build env vars.
4. **Do not touch the device upload path.** The `allowed_devices` MAC-FK anon-insert gate is
   shipping and proven. Viewer work is read/annotate on top of it.
5. **Raw CSV blob is immutable provenance.** `samples` is *derived* — always re-derivable by
   re-parsing the blob. Never edit a blob in place; never treat `samples` as source of truth.
6. **Reuse the portal chart renderer.** Port `parseCsv` / `decimate` / `miniChart` /
   `drawCharts` / `bandSvg` from `portal_page.h` into the viewer's chart component. Same
   look, same NaN-gap handling, same min/max-preserving decimation, same threshold bands.
7. **RLS on by default** on every new table; deny-all baseline, then add project-scoped
   policies. Verify each policy with the MCP read-only tool before moving on.

---

## Schema delta (migrations, on top of Phase 4)

`allowed_devices`, `dives`, storage bucket `dives` already exist. Add:

```
0005_projects.sql
  profiles(id uuid pk → auth.users, display_name, created_at)
  projects(id uuid pk, name, owner_id → profiles, archived bool default false, created_at)
  project_members(project_id → projects, user_id → profiles, role text check in
                  ('viewer','annotator','admin'), primary key(project_id,user_id))

0006_sites_dives.sql
  sites(id uuid pk, name, lat double, lon double, radius_m int default 50, created_at)
  alter dives add: project_id → projects, site_id → sites (nullable),
                   archived bool default false, label text,
                   started_at timestamptz, ended_at timestamptz, lat double, lon double
  -- backfill project_id for existing dives before setting NOT NULL

0007_samples.sql            (DECISION #1 = hybrid/samples)
  samples(
    dive_id → dives, seq int,           -- row index in dive = stable annotation anchor
    t_ms bigint, ts timestamptz,        -- CSV 'ms' and 'utc'
    submerged bool, poi bool,
    depth_m real,                       -- depth_m
    temp_c real,                        -- cels_T_C (fallback bar30T_C/poetT_mC at parse)
    ph real,                            -- pH
    orp_mv real,                        -- ORP_Eh_mV
    ec_mscm real,                       -- EC_mScm
    sal_psu real,                       -- sal_PSU
    cyc_conc real,                      -- cyc_conc (nullable)
    primary key(dive_id, seq)
  )
  -- raw channels (P_mbar,*_uV,ec_nA,cyc_V,bar30T_C,poetT_mC) stay in the blob only.

0008_annotations_comments.sql
  annotations(id uuid pk, dive_id → dives, seq int null,   -- null seq = whole-dive note
              author_id → profiles, kind text check in ('text','image'),
              body text, image_path text null, created_at)
  comments(id uuid pk, dive_id → dives, author_id → profiles, body text, created_at)
  storage bucket 'annotation-images' (private, authed-read, member-insert)

0009_thresholds.sql
  thresholds(project_id → projects null, metric text, warn_lo real, warn_hi real,
             crit_lo real, crit_hi real)   -- null project_id = global default

0010_noaa_normals.sql       (Phase 9)
  noaa_normals(lat_bin, lon_bin, month int, metric text, depth_bin int,
               mean real, stdev real, source text, fetched_at, primary key(...))
```

RLS pattern (all read/write tied to `project_members`):
```
using ( exists (select 1 from project_members m
                where m.project_id = <row's project_id> and m.user_id = auth.uid()) )
```
`samples` inherits its project via `dives`. Device anon-insert path is unchanged.

---

## Phases

### Phase 5 — Viewer MVP  (Screen 1 + Screen 2 core)
> **Session status (2026-07-20):** all Phase 5 *code* is written and the viewer **builds
> clean** (`npm run build`). What remains needs your Supabase/Cloudflare credentials and is
> listed in `cloud/apps/viewer/README.md` → "Go-live checklist" (db push, gen types, deploy
> parse-dive, create auth user, run `bootstrap_viewer.sql`, backfill samples, Cloudflare Pages).
- [x] `cloud/apps/viewer` Vite + React + TS scaffold; `@supabase/supabase-js`; router
- [~] Migrations 0005–0009 authored (projects/members/profiles, sites+dives, samples,
      annotations/comments, thresholds); `bootstrap_viewer.sql` creates a project + membership
      and backfills dives — **run `supabase db push` + the bootstrap script** (not yet applied)
- [~] `database.types.ts` hand-authored stub committed — **regenerate** via `supabase gen types`
      after push
- [~] Edge Function `parse-dive` written (blob → `samples`, temp fallback, idempotent on
      `(dive_id,seq)`, `all:true` backfill mode) — **deploy** it (`supabase functions deploy`)
- [x] **Login** — Supabase Auth email/password **+ Google OAuth**, `RequireAuth` guard on routes
- [x] **Dive graph screen**
  - [x] Calendar: days with logs marked, badge = # logs that day (UTC, month nav, day filter)
  - [x] Card: project's dive-log list; click selects a dive to graph
  - [x] Card: graph controls — metric visibility toggles; per-metric-scaled multi-metric view
        (ported `miniChart`/`bandSvg`/`decimate`); threshold-marker toggle
  - [x] Chart component reads the blob per DECISION #1 (works before parse-dive runs)
- [~] **Cloudflare Pages**: `public/_redirects` + build config documented in README — **connect
      the repo + set env** to deploy
- [ ] Smoke: log in → pick a real uploaded dive → graph renders with correct metrics/thresholds
      (needs the go-live steps above)

### Phase 6 — Map  (Screen 3)
- [ ] Migration 0006 sites; assign `site_id` by GPS proximity (`radius_m`) at parse time
- [ ] **Map** (MapLibre GL JS — no token) with a marker per site
- [ ] Click marker → open that site's dive graph; if >1 dive at the site, modal to pick
- [ ] Site created automatically on first dive with GPS outside all existing radii

### Phase 7 — Annotations & comments
- [ ] Migration 0008 + `annotation-images` bucket
- [ ] Click a point/region on the graph → add text annotation anchored to `(dive_id, seq)`
- [ ] Attach image to an annotation (upload to bucket, member-read)
- [ ] Comment thread on a dive (comment on other members' logs, per DECISION #2)
- [ ] Annotations render as pins on the graph at their `seq`

### Phase 8 — Validation / Comparison
- [ ] **Compare view**: overlay N selected dives on one time/depth axis (normalized bands)
- [ ] Dive vs project median/percentile for the same metric
- [ ] Dive vs dives at the same `site_id` (this-location history)
- [ ] Threshold markers driven by `thresholds` (project + global defaults)

### Phase 9 — NOAA normalization  (DECISION #3)
- [ ] Pick source (WOA climatology recommended); document the exact dataset/endpoint
- [ ] Edge Function (or CF Worker) fetch + cache into `noaa_normals`, keyed by site+month+metric
- [ ] Overlay the normal mean±stdev band behind a dive's temp/salinity trace

### Phase 10 — Management
- [ ] Copy dive (duplicate metadata + blob, new id, same/target project)
- [ ] Archive dive/project (`archived=true`, hidden from default lists, restorable)
- [ ] Move dive to a different project (reassign `project_id`; re-check membership/RLS)

### Later / value-add
- [ ] Calibration-health strip from `callog.csv` (needs a cal-log upload leg first)
- [ ] Public/shareable report links (DECISION #2 option c)
- [ ] Station-mode auto-upload fleet path (firmware stack)

---

## One-time setup (Claude Code)

**Cloudflare** — official validated prompt: `https://developers.cloudflare.com/agent-setup/prompt.md`
```
claude plugin marketplace add cloudflare/skills
claude plugin install cloudflare@cloudflare
# then: /reload-plugins   (activates skills + Cloudflare MCP servers)
```
- OAuth triggers on first Cloudflare tool use — sign in as **tech@isensys.com**.
- Installs MCPs: core, `-docs` (public, no auth), `-bindings`, `-builds`, `-observability`.
  Used to create the Pages project, wire the GitHub build, set env vars, and (Phase 9) the
  NOAA Worker. `-docs` is fine to leave on for reference.

**Supabase** — `.mcp.json` in `cloud/`, `http` type, **read-only**:
```json
{ "mcpServers": { "supabase": {
  "type": "http", "url": "https://mcp.supabase.com/mcp?read_only=true" } } }
```
OAuth browser flow on first use. Read/inspect/typegen only — see Hard Rule 2.

## Per-session workflow (in `cloud/`)
```
supabase link                    # once
# edit migrations → supabase db push        (schema changes: migrations ONLY)
supabase gen types typescript --linked > apps/viewer/src/database.types.ts
# Supabase MCP (read-only): list tables, run a SELECT, confirm an RLS policy, inspect a row
supabase functions deploy parse-dive        # Edge Functions
# viewer: pnpm dev  /  git push → Cloudflare Pages auto-deploy (build env: SUPABASE_URL + anon key)
```

## Acceptance (MVP = Phases 5–6)
- [ ] A logged-in member sees only their projects' dives (RLS proven with MCP as a non-member)
- [ ] Any uploaded dive graphs correctly with per-metric scaling + threshold markers
- [ ] Map shows one marker per site; multi-dive site opens the picker modal
- [ ] Secret key appears nowhere in the viewer bundle, browser, or git history
