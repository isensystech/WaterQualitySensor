# DiveViewer Phase 5 — smoke test

Run these **in order**. Each step has a one-line **expected result** — if it doesn't match,
stop and bring me the output. Green build has not touched the real DB yet; a clean push plus
one RLS round-trip is the actual Phase-5 checkpoint (that's when we commit).

Supabase commands run from `cloud/` (needs `supabase link` done once). The viewer build runs
from `cloud/apps/viewer/`. `curl` checks use only the **public** URL + publishable key:

```bash
export SB_URL="https://gwaxsksjierpzbxugbxj.supabase.co"
export ANON="sb_publishable_f6xnKDgjeStNYV-H19VPtg_WqOcwiCw"
# (jq is used to pull tokens; `brew install jq` if missing)
```

---

## A. Deploy chain (dependency order)

### 1. Push the schema
```bash
cd cloud
supabase db push          # applies 0005_projects … 0010_dives_anon_insert_only
```
**Expected:** `Applying migration 0005_projects.sql … 0010_dives_anon_insert_only.sql` then
`Finished supabase db push.` — **no error**. (0010 is last and hardens anon on `dives` to
INSERT-only — see section D for what it changes and the re-check that it didn't overreach.)
⚠️ The single privilege-dependent line is in 0005: `create trigger on_auth_user_created on
auth.users` (the standard Supabase profile-autocreate pattern; this project already creates
`storage.objects` policies in 0001, which needs the same elevated rights, so it should pass).
If push fails **specifically** with `must be owner of relation users`, stop and tell me — I'll
move profile creation to the client and drop the trigger.

### 2. Regenerate types (replaces the hand-authored stub)
```bash
supabase gen types typescript --linked > apps/viewer/src/database.types.ts
git diff --stat apps/viewer/src/database.types.ts
```
**Expected:** the file changes; it now contains `projects`, `project_members`, `samples`,
`annotations`, `thresholds`, and the `dives` additions (`project_id`, `started_at`, …).
Then re-verify the app still compiles:
```bash
cd apps/viewer && npm run build && cd ..
```
**Expected:** `✓ built` with no TS errors. (If gen-types names a column differently than the
stub, this is where it surfaces — bring me any TS error.)

### 3. Deploy the parser
```bash
supabase functions deploy parse-dive
supabase secrets set PARSE_DIVE_SECRET="$(openssl rand -hex 16)"   # optional but recommended
supabase secrets list | grep PARSE_DIVE_SECRET
```
**Expected:** `Deployed Function parse-dive`; the secret appears in the list. Save the secret
value — you'll send it as `x-parse-secret` in step 6.

### 4. Create your login
Dashboard → Authentication → Add user (email + password), **or** enable Google OAuth.
```bash
export EMAIL="you@example.com"; export PASS="your-password"
JWT=$(curl -s "$SB_URL/auth/v1/token?grant_type=password" \
  -H "apikey: $ANON" -H "content-type: application/json" \
  -d "{\"email\":\"$EMAIL\",\"password\":\"$PASS\"}" | jq -r .access_token)
echo "${JWT:0:12}…"
```
**Expected:** prints `eyJhbGciOi…` (a real JWT). If it prints `null`, the user/password is
wrong — fix before continuing.

### 5. Bootstrap membership + backfill dives
Edit the email in `supabase/scripts/bootstrap_viewer.sql` first, then:
```bash
supabase db execute --file supabase/scripts/bootstrap_viewer.sql
```
**Expected:** `NOTICE: Bootstrap OK: you@example.com -> project <uuid> (N dives now in
project)` with **N = 4** (the four existing dives), and no exception.

### 6. Backfill `samples`
```bash
curl -s -X POST "$SB_URL/functions/v1/parse-dive" \
  -H "content-type: application/json" \
  -H "x-parse-secret: <the-secret-from-step-3>" \
  -d '{"all":true}' | jq
```
**Expected:** `{"parsed": N, "results": [ {"dive_id":"…","rows": R}, … ]}` — every result has
a numeric `rows` and **no `error`** field. (If a dive row has no blob in storage it'll show an
`error` there; the 4 real dives should all have `rows`.)

### 7. Parse sanity — samples count matches the CSV
In the SQL editor (or Supabase MCP), per-dive counts should line up:
```sql
select d.filename, d.row_count, count(s.*) as sample_rows
from public.dives d left join public.samples s on s.dive_id = d.id
where d.project_id is not null
group by d.filename, d.row_count
order by d.filename;
```
**Expected:** `sample_rows = row_count` for every dive, and `> 0`.
Rigorous cross-check against the raw blob for the synthetic dive:
```bash
curl -s "$SB_URL/storage/v1/object/authenticated/dives/f412fa000001/dive0000.csv" \
  -H "apikey: $ANON" -H "Authorization: Bearer $JWT" \
  | grep -v '^#' | grep -v '^$' | tail -n +2 | wc -l
```
**Expected:** the printed count equals `sample_rows` for `dive0000.csv` (data lines = total −
`#`meta − 1 header).

### 8. Deploy to Cloudflare Pages
Connect the GitHub repo (account `tech@isensys.com`): root dir `cloud/apps/viewer`, build
`npm run build`, output `dist`, build env `VITE_SUPABASE_URL` + `VITE_SUPABASE_ANON_KEY`. Add
the Pages URL to Supabase Auth → URL Configuration (and the Google OAuth client if using it).
**Expected:** build succeeds; visiting the Pages URL loads the app and (logged out) shows the
sign-in card.

---

## B. RLS round-trip — "hidden until assigned," not "hidden forever"
This is the one thing the local-SQL gap couldn't test: proves a `project_id IS NULL` dive is
invisible to a member, then **becomes visible once assigned**. Run after step 5.

### B1. Insert an unassigned dive via the anon **device** path
Use `Prefer: return=minimal` — the device's plain-POST contract. After **0010** anon has INSERT
but **not** SELECT, so `return=representation` (which does `INSERT … RETURNING`) would 401; the
device never asks for the row back. Confirm the insert via the service role in B2, not here.
```bash
curl -s -o /dev/null -w "HTTP %{http_code}\n" -X POST "$SB_URL/rest/v1/dives" \
  -H "apikey: $ANON" -H "content-type: application/json" -H "Prefer: return=minimal" \
  -d '{"device_id":"f412fa000001","filename":"rls-test.csv","storage_path":"f412fa000001/rls-test.csv"}'
```
**Expected:** `HTTP 201` (empty body). No blob is uploaded — this tests row visibility only.

### B2. Prove it EXISTS but is HIDDEN
Member view (RLS applies):
```bash
curl -s "$SB_URL/rest/v1/dives?filename=eq.rls-test.csv&select=id,project_id" \
  -H "apikey: $ANON" -H "Authorization: Bearer $JWT" | jq
```
**Expected:** `[]` — hidden from the member.
Service-role view (SQL editor / MCP, bypasses RLS) confirms the row is really there:
```sql
select id, project_id from public.dives where filename = 'rls-test.csv';
```
**Expected:** 1 row, `project_id = null`. → "hidden," not "insert dropped."

### B3. Assign it to your project (no assign-UI yet — that's Phase 10)
```sql
update public.dives
set project_id = (select id from public.projects where name = 'Demo Project' order by created_at limit 1)
where filename = 'rls-test.csv';
```
**Expected:** `UPDATE 1`.

### B4. Member now SEES it
```bash
curl -s "$SB_URL/rest/v1/dives?filename=eq.rls-test.csv&select=id,project_id" \
  -H "apikey: $ANON" -H "Authorization: Bearer $JWT" | jq
```
**Expected:** 1 row with a non-null `project_id`. → **round-trip proven.**

### B5. Cleanup
```sql
delete from public.dives where filename = 'rls-test.csv';
```
**Expected:** `DELETE 1`. (Don't run `parse-dive` on this row — it has no blob.)

---

## C. Auth guard (UI bounce + server-side)

### C1. UI — logged out is bounced to /login
In a **private/incognito** window, open the app root (dev `npm run dev` → http://localhost:5173,
or the Pages URL) and also try navigating straight to `/`.
**Expected:** you land on `/login` with the sign-in card; **no dive list or data** renders
until you sign in.

### C2. Server-side — anon can't read dives (grant + RLS both deny)
After **0010**, anon's SELECT grant on `dives` is revoked, so the request fails at the
**privilege layer** (Postgres 42501) before RLS is even consulted — the grant layer and the RLS
layer now agree (RLS would also empty it). Belt-and-suspenders on the one anon-writable table.
```bash
curl -s -w "\nHTTP %{http_code}\n" "$SB_URL/rest/v1/dives?select=id&limit=5" -H "apikey: $ANON"
```
**Expected:** `HTTP 401` with a `permission denied for table dives` body (code `42501`). If you
instead get `200 []`, **0010 didn't apply** — check the `db push` output before continuing.
(Context: this project's default privileges hand anon full CRUD on every *other* new table;
those stay RLS-only for now, so a 200 [] there would be normal — 0010 only tightens `dives`.)

---

## D. Post-0010 re-check — did the anon revoke overreach? (run LAST, after A–C)
**0010** is `revoke all on public.dives from anon;` + `grant insert on public.dives to anon;`.
It makes the **grant layer agree with the RLS layer** on the one anon-writable table —
belt-and-suspenders on `dives`, the same defense-in-depth pattern used everywhere else in the
project. The revoke targets **anon only**, so these two re-runs prove it didn't overreach into
the device path (anon INSERT) or the member path (authenticated SELECT). Uses a fresh throwaway
row so it's independent of section B's cleanup.

### D1. anon INSERT still works (device path intact)
`return=minimal` again — post-0010 `return=representation` would 401 (no anon SELECT) and read
as a false overreach, so use the device's real plain-POST shape.
```bash
curl -s -o /dev/null -w "HTTP %{http_code}\n" -X POST "$SB_URL/rest/v1/dives" \
  -H "apikey: $ANON" -H "content-type: application/json" -H "Prefer: return=minimal" \
  -d '{"device_id":"f412fa000001","filename":"rls-test2.csv","storage_path":"f412fa000001/rls-test2.csv"}'
```
**Expected:** `HTTP 201`. → anon kept exactly the INSERT it needs; the revoke didn't touch it.

### D2. assign it to your project (SQL editor / service role)
```sql
update public.dives
set project_id = (select id from public.projects where name = 'Demo Project' order by created_at limit 1)
where filename = 'rls-test2.csv';
```
**Expected:** `UPDATE 1`.

### D3. member (JWT) still SEES the assigned dive (authenticated grants independent of anon)
```bash
curl -s "$SB_URL/rest/v1/dives?filename=eq.rls-test2.csv&select=id,project_id" \
  -H "apikey: $ANON" -H "Authorization: Bearer $JWT" | jq
```
**Expected:** **1 row** with a non-null `project_id`. → the `authenticated` role's SELECT grant
is untouched; revoking anon did **not** overreach.
⚠️ **If this comes back `[]` empty, the revoke caught more than anon — STOP and bring it to me.**

### D4. cleanup
```sql
delete from public.dives where filename = 'rls-test2.csv';
```
**Expected:** `DELETE 1`.

## Done = commit
All A steps clean, B round-trip flips `[]` → 1 row on assignment, C1 bounces to /login, C2
returns **`401`** (0010's grant-level block), and D re-check passes (D1 still `201`, D3 still
**1 row**) → Phase 5 is real. **Then** commit (migrations incl. `0010` + `apps/viewer/` +
`parse-dive` + config + docs).
