# Contributing to the WQL Dive Viewer — frontend / design

Welcome, Kate! This app is the **web viewer** for dive logs uploaded by the water-quality
loggers. It's a Vite + React + TypeScript SPA talking to Supabase (Postgres + Auth + Storage).
Your focus is **the looks** — this doc gets you running against real data in a few minutes and
shows you where the visual layer lives and what's safe to change.

**Design direction:** this is a *web* app — treat it like one. Aim for a crisp, clean, modern
interface. It is **not** bound by the logger's 240 px device screen, and the chart renderer that
was ported from the firmware is **not** a design constraint — restyle it freely, introduce a
design system, swap the chart look, whatever reads best on the web.

## 1. Run it

**Zero-setup (GitHub Codespaces — recommended):** on the repo page → **Code → Codespaces → Create
codespace on main**. The devcontainer installs deps and seeds `.env.local` for you; then in the
terminal:
```bash
cd cloud/apps/viewer && npm run dev     # the port-5173 preview opens automatically
```

**Local:**
```bash
git clone https://github.com/isensystech/WaterQualitySensor.git
cd WaterQualitySensor/cloud/apps/viewer
cp .env.example .env.local        # already points at the live project; both values are PUBLIC
npm install
npm run dev                       # http://localhost:5173  (hot reload)
```
`npm run build` runs `tsc --noEmit && vite build` — keep it green before pushing.

## 2. See real data (Supabase Auth + RLS)
The app shows **nothing** until you sign in AND your account is a member of a project — row-level
security scopes every dive to `project_members`. Two ways in:
- **Your own account (preferred):** sign up on the login screen, then ask Scott to add you to a
  project (one SQL line / the `supabase/scripts/bootstrap_viewer.sql` flow). Then you'll see the
  demo dives with samples + POIs to style against.
- **Shared demo login:** ask Scott for the `phase5-smoke@…` credentials for a quick look.

No account/data yet? The UI still renders (empty states) so you can style most of it offline.

## 3. Where the looks live
| File | What it styles |
|---|---|
| `src/index.css` | **The whole stylesheet** — dark-theme tokens, layout, cards, chips, modal, map. Start here. |
| `src/lib/chart.ts` | Chart geometry + the metric **colors** (`CSER`) and viewBox constants. |
| `src/components/DiveCharts.tsx` | The interactive single-column charts (crosshair, hover readout, POI markers). |
| `src/components/DiveMeta.tsx` | Dive-metadata pictogram chips + tooltips. |
| `src/components/MapCard.tsx` | The MapLibre map + markers/popups. |
| `src/components/Calendar.tsx` | The dive calendar. |
| `src/components/AnnotationModal.tsx` | POI Name/Description/Photos modal. |
| `src/screens/DiveGraph.tsx` | Page layout (map + calendar + list + chart panel). |
| `src/components/DiveChart.tsx` | Chart container: metadata + metric toggles + charts + modal. |

There's no CSS framework — it's one hand-written stylesheet. Adding Tailwind / CSS-modules / a
component lib is your call; just keep the build green and the bundle sane.

## 4. Guardrails (functional, not visual — please don't break these)
- **Auth/RLS:** data access is gated by Supabase Auth + project membership. Don't disable the
  route guard (`RequireAuth`) or the login flow.
- **Keys:** only the **publishable/anon** key ships in the bundle. Never commit a secret key;
  `.env.local` is gitignored — keep it that way (this repo is public).
- **Data mapping:** the CSV→metric mapping lives in `chart.ts` `CSER` (which column feeds Depth,
  Temp, pH, …). Restyling is free; if you change *which* column maps to a metric, check it against
  a real dive. Dive/sample/annotation field names come from the DB schema — don't rename them
  client-side.
- **Chart math:** decimation + NaN-gap handling in `chart.ts` exist so long dives render honestly
  (spikes survive, sensor-off gaps stay gaps). Keep that behavior if you rework the charts.

## 5. Commit + publish
You and Scott commit **straight to `main`** (no PRs). Before pushing, make sure it builds:
```bash
npm run build                       # tsc + vite — keep it green
git pull --rebase                   # get Scott's latest first
git add -A && git commit -m "looks: …" && git push
```

### Publish to the live site
Live URL: **https://wql-dive-viewer.still-glitter-44b0.workers.dev** (Cloudflare Workers static
assets). Pushing to `main` does **not** auto-deploy — publishing is one manual command:
```bash
export CLOUDFLARE_API_TOKEN="<token from Scott>"   # account-scoped; do NOT commit it
npm run deploy                                      # = vite build + wrangler deploy
```
The account id is already in `wrangler.jsonc`, so the token is the only thing you need to set.
**Or just tell Claude Code: "redeploy the viewer"** — it runs `npm run deploy` from
`cloud/apps/viewer`. Questions → Scott.
