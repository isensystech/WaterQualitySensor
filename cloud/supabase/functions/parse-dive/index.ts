// parse-dive — explode a dive's immutable CSV blob into the `samples` table (DECISION #1).
// Runs with the SERVICE-ROLE key (bypasses RLS) so it can read private storage + write
// samples. Idempotent per dive: delete-then-insert keyed on (dive_id, seq).
//
// Invoke (POST JSON):
//   { "dive_id": "<uuid>" }        parse one dive
//   { "all": true, "limit": 50 }   backfill: parse every dive that has no samples yet
// Optional shared secret: if PARSE_DIVE_SECRET is set in the function env, callers must
// send it as the `x-parse-secret` header (this function is deployed verify_jwt=false).
//
// CSV shape (firmware writeLogRow): '#'-prefixed meta header, then one column header line
//   ms,utc,submerged,poi,P_mbar,depth_m,bar30T_C,poetT_mC,ugs_uV,orp_uV,ec_nA,ec_uV,pH,
//   EC_mScm,sal_PSU,ORP_Eh_mV,cyc_V,cyc_conc,cels_T_C
// then data rows; empty cell = no reading; utc is ISO-8601 UTC or the literal 'unsynced'.

import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

const CHUNK = 1000; // rows per insert — keeps request bodies well under limits

type Parsed = {
  meta: string[];
  cols: string[];
  idx: Record<string, number>;
  rows: string[][];
};

function parseCsv(text: string): Parsed {
  const lines = text.split(/\r?\n/);
  const meta: string[] = [];
  let cols: string[] | null = null;
  const rows: string[][] = [];
  for (const ln of lines) {
    if (ln === "") continue;
    if (ln.charAt(0) === "#") { meta.push(ln.replace(/^#\s?/, "")); continue; }
    if (!cols) { cols = ln.split(","); continue; }
    rows.push(ln.split(","));
  }
  const idx: Record<string, number> = {};
  (cols ?? []).forEach((c, i) => { idx[c] = i; });
  return { meta, cols: cols ?? [], idx, rows };
}

const numOrNull = (v: string | undefined): number | null => {
  if (v == null || v === "") return null;
  const n = Number(v);
  return Number.isFinite(n) ? n : null;
};
const boolOrNull = (v: string | undefined): boolean | null => {
  if (v == null || v === "") return null;
  return v.trim() === "1";
};
// utc column -> ISO string, or null for 'unsynced'/anything unparseable.
const tsOrNull = (v: string | undefined): string | null => {
  if (!v || v === "unsynced") return null;
  const d = new Date(v);
  return Number.isNaN(d.getTime()) ? null : d.toISOString();
};

// temp_c source-of-truth at parse time: cels_T_C -> bar30T_C -> poetT_mC/1000
function tempC(row: string[], idx: Record<string, number>): number | null {
  const cels = numOrNull(row[idx["cels_T_C"]]);
  if (cels != null) return cels;
  const bar = numOrNull(row[idx["bar30T_C"]]);
  if (bar != null) return bar;
  const poet = numOrNull(row[idx["poetT_mC"]]);
  return poet != null ? poet / 1000 : null;
}

async function parseOne(admin: ReturnType<typeof createClient>, diveId: string) {
  const { data: dive, error: dErr } = await admin
    .from("dives").select("id, storage_path, utc_start").eq("id", diveId).single();
  if (dErr || !dive) throw new Error(`dive ${diveId} not found: ${dErr?.message ?? ""}`);

  const dl = await admin.storage.from("dives").download(dive.storage_path as string);
  if (dl.error || !dl.data) throw new Error(`blob download failed: ${dl.error?.message}`);
  const P = parseCsv(await dl.data.text());

  const has = (c: string) => P.idx[c] != null;
  const records = P.rows.map((r, seq) => ({
    dive_id: diveId,
    seq,
    t_ms: has("ms") ? numOrNull(r[P.idx["ms"]]) : null,
    ts: has("utc") ? tsOrNull(r[P.idx["utc"]]) : null,
    submerged: has("submerged") ? boolOrNull(r[P.idx["submerged"]]) : null,
    poi: has("poi") ? boolOrNull(r[P.idx["poi"]]) : null,
    depth_m: has("depth_m") ? numOrNull(r[P.idx["depth_m"]]) : null,
    temp_c: tempC(r, P.idx),
    ph: has("pH") ? numOrNull(r[P.idx["pH"]]) : null,
    orp_mv: has("ORP_Eh_mV") ? numOrNull(r[P.idx["ORP_Eh_mV"]]) : null,
    ec_mscm: has("EC_mScm") ? numOrNull(r[P.idx["EC_mScm"]]) : null,
    sal_psu: has("sal_PSU") ? numOrNull(r[P.idx["sal_PSU"]]) : null,
    cyc_conc: has("cyc_conc") ? numOrNull(r[P.idx["cyc_conc"]]) : null,
  }));

  // Idempotent: clear then re-insert (blob is immutable, so this is deterministic and also
  // drops any stale higher-seq rows from an earlier parse).
  const del = await admin.from("samples").delete().eq("dive_id", diveId);
  if (del.error) throw new Error(`samples clear failed: ${del.error.message}`);
  for (let i = 0; i < records.length; i += CHUNK) {
    const ins = await admin.from("samples").insert(records.slice(i, i + CHUNK));
    if (ins.error) throw new Error(`samples insert failed @${i}: ${ins.error.message}`);
  }

  // Roll dive summary fields forward from the parsed data.
  const tss = records.map((r) => r.ts).filter((t): t is string => !!t).sort();
  const patch: Record<string, unknown> = { row_count: records.length };
  patch.started_at = tss[0] ?? dive.utc_start ?? null;
  patch.ended_at = tss.length ? tss[tss.length - 1] : null;
  await admin.from("dives").update(patch).eq("id", diveId);

  return { dive_id: diveId, rows: records.length };
}

Deno.serve(async (req) => {
  if (req.method !== "POST") return json({ error: "POST only" }, 405);

  const secret = Deno.env.get("PARSE_DIVE_SECRET");
  if (secret && req.headers.get("x-parse-secret") !== secret) {
    return json({ error: "forbidden" }, 403);
  }

  const admin = createClient(
    Deno.env.get("SUPABASE_URL")!,
    Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!,
    { auth: { persistSession: false } },
  );

  let body: { dive_id?: string; all?: boolean; limit?: number } = {};
  try { body = await req.json(); } catch { /* empty body ok */ }

  try {
    if (body.dive_id) {
      return json(await parseOne(admin, body.dive_id));
    }
    if (body.all) {
      // dives with zero samples yet (left-anti-join via a not-in on distinct sample dives)
      const limit = Math.min(body.limit ?? 50, 500);
      const { data: parsed } = await admin.from("samples").select("dive_id");
      const done = new Set((parsed ?? []).map((r) => r.dive_id as string));
      const { data: dives, error } = await admin
        .from("dives").select("id").order("uploaded_at", { ascending: true }).limit(1000);
      if (error) throw new Error(error.message);
      const todo = (dives ?? []).map((d) => d.id as string)
        .filter((id) => !done.has(id)).slice(0, limit);
      const results = [];
      for (const id of todo) {
        try { results.push(await parseOne(admin, id)); }
        catch (e) { results.push({ dive_id: id, error: String((e as Error).message) }); }
      }
      return json({ parsed: results.length, results });
    }
    return json({ error: "provide dive_id or all:true" }, 400);
  } catch (e) {
    return json({ error: String((e as Error).message) }, 500);
  }
});

function json(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}
