// dives.ts — data access for the dive list / graph. All reads are RLS-scoped to the
// signed-in user's project memberships (migrations 0006/0007), so the queries are plain
// selects with no client-side project filtering needed.
import { supabase } from "../supabase";
import type { Database } from "../database.types";
import type { Band } from "./chart";

export type Dive = Database["public"]["Tables"]["dives"]["Row"];
export type Threshold = Database["public"]["Tables"]["thresholds"]["Row"];

// The timestamp a dive is filed under on the calendar (UTC date): prefer the parsed/started
// time, then the device-reported start, then upload time as a last resort for unsynced dives.
export function diveInstant(d: Dive): string | null {
  return d.started_at ?? d.utc_start ?? d.uploaded_at ?? null;
}

// UTC yyyy-mm-dd key (dives are logged in UTC; keep the calendar in the same frame).
export function dayKey(iso: string | null): string | null {
  if (!iso) return null;
  const t = new Date(iso);
  return Number.isNaN(t.getTime()) ? null : t.toISOString().slice(0, 10);
}

export async function fetchDives(): Promise<Dive[]> {
  const { data, error } = await supabase
    .from("dives")
    .select("*")
    .eq("archived", false)
    .order("started_at", { ascending: false, nullsFirst: false })
    .order("uploaded_at", { ascending: false });
  if (error) throw error;
  return data ?? [];
}

export async function fetchDeviceLabels(): Promise<Record<string, string>> {
  const { data, error } = await supabase.from("allowed_devices").select("mac,label");
  if (error) throw error;
  const m: Record<string, string> = {};
  (data ?? []).forEach((d) => { m[d.mac] = d.label || d.mac; });
  return m;
}

export async function fetchThresholds(): Promise<Threshold[]> {
  const { data, error } = await supabase.from("thresholds").select("*");
  if (error) throw error;
  return data ?? [];
}

// Resolve metric -> Band for one dive: a project-specific row overrides the global (null) row.
export function thresholdsForDive(all: Threshold[], projectId: string | null): Record<string, Band> {
  const out: Record<string, Band> = {};
  const pick = (metric: string) => {
    const proj = all.find((t) => t.metric === metric && t.project_id === projectId);
    const glob = all.find((t) => t.metric === metric && t.project_id === null);
    return proj ?? glob;
  };
  for (const metric of ["depth", "temp", "ph", "ec", "sal", "orp", "cyc"]) {
    const t = pick(metric);
    if (t) out[metric] = { wlo: t.warn_lo, whi: t.warn_hi, alo: t.crit_lo, ahi: t.crit_hi };
  }
  return out;
}

export async function downloadDiveCsv(storagePath: string): Promise<string> {
  const { data, error } = await supabase.storage.from("dives").download(storagePath);
  if (error || !data) throw error ?? new Error("download failed");
  return await data.text();
}

export function fmtUtc(s: string | null): string {
  if (!s) return "unsynced";
  const d = new Date(s);
  return Number.isNaN(d.getTime())
    ? s
    : d.toISOString().replace("T", " ").replace(/:\d\d\..*/, "") + " UTC";
}
