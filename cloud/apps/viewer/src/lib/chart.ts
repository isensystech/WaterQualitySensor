// chart.ts — chart data-prep + geometry, ported from firmware/src/portal_page.h (Hard Rule 6:
// same NaN-gap handling, same min/max-preserving decimation, same threshold bands). v2 returns
// a structured model (not SVG strings) so the React chart can add a synced crosshair + hover
// readout while keeping the exact look. Keep the metric list / colors in sync with portal_page.h.

// High-resolution coordinate space (the firmware portal used a tiny 336x132 box for a 240px
// TFT; stretched to full width that looked upscaled/chunky). A ~1000-wide viewBox renders at
// ~1:1 on screen, so strokes/fonts are crisp and the decimation target is wide = smoother lines.
export const CVW = 1000, CHH = 220, CpadL = 54, CpadR = 16, CpadT = 26, CpadB = 24;
export const CpW = CVW - CpadL - CpadR, CpH = CHH - CpadT - CpadB;

export type Pt = [number, number];
export interface Band { wlo?: number | null; whi?: number | null; alo?: number | null; ahi?: number | null }

export interface ParsedCsv {
  meta: string[];
  cols: string[];
  idx: Record<string, number>;
  rows: number[][];
}

interface MetricDef {
  k: string;
  cols: [string, number][];
  lab: string;
  u: string;
  col: string;
  inv?: number;
  band?: string;
  opt?: number;
}

export const CSER: MetricDef[] = [
  { k: "depth", cols: [["depth_m", 1]], lab: "Depth", u: "m", col: "#58a6ff", inv: 1, band: "depth" },
  { k: "temp", cols: [["cels_T_C", 1], ["bar30T_C", 1], ["poetT_mC", 0.001]], lab: "Temp", u: "°C", col: "#ff8c5a", band: "temp" },
  { k: "ph", cols: [["pH", 1]], lab: "pH", u: "", col: "#c98bff", band: "ph" },
  { k: "ec", cols: [["EC_mScm", 1]], lab: "Conductivity", u: "mS/cm", col: "#5dd6c0", band: "ec" },
  { k: "sal", cols: [["sal_PSU", 1]], lab: "Salinity", u: "PSU", col: "#7fd6ff", band: "sal" },
  { k: "orp", cols: [["ORP_Eh_mV", 1]], lab: "ORP", u: "mV", col: "#e0c060", band: "orp" },
  { k: "cyc", cols: [["cyc_conc", 1]], lab: "Fluorometry", u: "", col: "#6ee07a", band: "cyc", opt: 1 },
];

export const CRAW: { col: string; lab: string; u: string }[] = [
  { col: "P_mbar", lab: "Pressure", u: "mbar" },
  { col: "ugs_uV", lab: "pH raw (Ugs)", u: "µV" },
  { col: "orp_uV", lab: "ORP raw", u: "µV" },
  { col: "ec_nA", lab: "EC current", u: "nA" },
  { col: "ec_uV", lab: "EC voltage", u: "µV" },
  { col: "cyc_V", lab: "Cyclops raw", u: "V" },
];

const Nn = (x: string | undefined): number => (x == null || x === "" ? NaN : +x);
const clmp = (v: number, a: number, b: number) => (v < a ? a : v > b ? b : v);
export const fmtNum = (v: number): string => {
  if (isNaN(v)) return "—";
  const a = Math.abs(v);
  if (a >= 1000) return v.toFixed(0);
  if (a >= 100) return v.toFixed(1);
  if (a >= 1) return v.toFixed(2);
  return v.toFixed(3);
};
export const fmtT = (ms: number): string => {
  let s = Math.round(ms / 1000);
  const m = Math.floor(s / 60);
  s -= m * 60;
  return m + ":" + (s < 10 ? "0" : "") + s;
};

export function parseCsv(t: string): ParsedCsv {
  const L = t.split(/\r?\n/), meta: string[] = [], rows: number[][] = [];
  let cols: string[] | null = null;
  for (const ln of L) {
    if (ln === "") continue;
    if (ln.charAt(0) === "#") { meta.push(ln.replace(/^#\s?/, "")); continue; }
    if (!cols) { cols = ln.split(","); continue; }
    const c = ln.split(","), r = new Array<number>(cols.length);
    for (let j = 0; j < cols.length; j++) r[j] = Nn(c[j]);
    rows.push(r);
  }
  const idx: Record<string, number> = {};
  if (cols) for (let i = 0; i < cols.length; i++) idx[cols[i]] = i;
  return { meta, cols: cols || [], rows, idx };
}

// full per-sample values for the first column with any data (temp fallback list); NaN = gap
function seriesFull(P: ParsedCsv, cols: [string, number][]): number[] | null {
  for (const [name, sc] of cols) {
    const i = P.idx[name];
    if (i == null) continue;
    const out: number[] = [];
    let any = false;
    for (let r = 0; r < P.rows.length; r++) {
      const v = P.rows[r][i];
      if (!isNaN(v)) { any = true; out.push(v * sc); } else out.push(NaN);
    }
    if (any) return out;
  }
  return null;
}

// min/max bucket per x-pixel: an out-of-band spike must survive decimation
function decimate(pts: Pt[], W: number): { pts: Pt[]; dec: boolean } {
  if (pts.length <= W * 2) return { pts, dec: false };
  const bs = Math.ceil(pts.length / W), out: Pt[] = [];
  for (let i = 0; i < pts.length; i += bs) {
    const hi = Math.min(i + bs, pts.length);
    let mn = Infinity, mx = -Infinity, mnx = 0, mxx = 0, any = false;
    for (let j = i; j < hi; j++) {
      const y = pts[j][1];
      if (isNaN(y)) continue;
      any = true;
      if (y < mn) { mn = y; mnx = pts[j][0]; }
      if (y > mx) { mx = y; mxx = pts[j][0]; }
    }
    if (!any) { out.push([pts[i][0], NaN]); continue; }
    if (mnx <= mxx) { out.push([mnx, mn]); if (mxx !== mnx) out.push([mxx, mx]); }
    else { out.push([mxx, mx]); out.push([mnx, mn]); }
  }
  return { pts: out, dec: true };
}

// ----- geometry helpers (shared so the crosshair maps identically on every stacked chart) -----
export const xPix = (x: number, xMin: number, xMax: number): number =>
  CpadL + ((x - xMin) / ((xMax - xMin) || 1)) * CpW;
export const yPix = (v: number, yMin: number, yMax: number, inv: boolean): number => {
  const rng = (yMax - yMin) || 1;
  return inv ? CpadT + ((v - yMin) / rng) * CpH : CpadT + ((yMax - v) / rng) * CpH;
};

export function bandRects(b: Band | undefined, yMin: number, yMax: number, inv: boolean):
  { y: number; h: number; fill: string }[] {
  if (!b) return [];
  const wlo = b.wlo ?? NaN, whi = b.whi ?? NaN, alo = b.alo ?? NaN, ahi = b.ahi ?? NaN;
  const R = "rgba(255,59,48,.20)", A = "rgba(255,164,0,.18)";
  const out: { y: number; h: number; fill: string }[] = [];
  const rect = (loV: number, hiV: number, fill: string) => {
    loV = clmp(loV, yMin, yMax); hiV = clmp(hiV, yMin, yMax);
    if (loV >= hiV) return;
    const y1 = yPix(hiV, yMin, yMax, inv), y2 = yPix(loV, yMin, yMax, inv);
    const top = Math.min(y1, y2), h = Math.abs(y2 - y1);
    if (h < 0.5) return;
    out.push({ y: top, h, fill });
  };
  if (!isNaN(ahi)) rect(ahi, yMax, R);
  if (!isNaN(whi)) rect(whi, isNaN(ahi) ? yMax : ahi, A);
  if (!isNaN(wlo)) rect(isNaN(alo) ? yMin : alo, wlo, A);
  if (!isNaN(alo)) rect(yMin, alo, R);
  return out;
}

// SVG path for a NaN-gapped line in pixel space
export function linePath(pts: Pt[], xMin: number, xMax: number, yMin: number, yMax: number, inv: boolean): string {
  let d = "", pen = false;
  for (const p of pts) {
    if (isNaN(p[1])) { pen = false; continue; }
    d += (pen ? "L" : "M") + xPix(p[0], xMin, xMax).toFixed(1) + " " + yPix(p[1], yMin, yMax, inv).toFixed(1) + " ";
    pen = true;
  }
  return d;
}

export interface MetricModel {
  key: string; label: string; unit: string; color: string; inv: boolean;
  band?: Band;
  full: number[];       // value per sample index (scaled, NaN = gap) — hover readout + dot
  drawPts: Pt[];        // decimated points for the line
  yMin: number; yMax: number;
  decimated: boolean;
}
export interface ChartModel {
  X: number[];          // ms per sample index (x axis)
  xMin: number; xMax: number;
  pois: number[];       // sample indices flagged poi=1
  metrics: MetricModel[];
  skipped: string[];
}
export interface BuildOpts {
  enabled: Set<string>;
  showRaw: boolean;
  showThresholds: boolean;
  thresholds: Record<string, Band>;
  cyclopsUnits?: string;
}

export function buildChartModel(D: ParsedCsv, opts: BuildOpts): ChartModel {
  const mi = D.idx["ms"], pi = D.idx["poi"];
  const n = D.rows.length;
  const X = new Array<number>(n), pois: number[] = [];
  for (let r = 0; r < n; r++) {
    const xv = mi != null ? D.rows[r][mi] : NaN;
    X[r] = isNaN(xv) ? r : xv;
    if (pi != null && D.rows[r][pi] === 1) pois.push(r);
  }
  let xMin = X[0] ?? 0, xMax = X[n - 1] ?? 1;
  if (!(xMax > xMin)) xMax = xMin + 1;

  const metrics: MetricModel[] = [], skipped: string[] = [];
  const push = (def: { key: string; label: string; unit: string; color: string; inv: boolean; band?: Band; cols: [string, number][]; opt?: boolean }) => {
    const full = seriesFull(D, def.cols);
    if (!full) { if (!def.opt) skipped.push(def.label); return; }
    let yMin = Infinity, yMax = -Infinity;
    for (const v of full) if (!isNaN(v)) { if (v < yMin) yMin = v; if (v > yMax) yMax = v; }
    if (yMin === Infinity) { yMin = 0; yMax = 1; }
    if (yMin === yMax) { yMin -= 1; yMax += 1; }
    const pd = (yMax - yMin) * 0.08; yMin -= pd; yMax += pd;
    const dd = decimate(full.map((v, r) => [X[r], v] as Pt), CpW);
    metrics.push({ key: def.key, label: def.label, unit: def.unit, color: def.color, inv: def.inv, band: def.band, full, drawPts: dd.pts, yMin, yMax, decimated: dd.dec });
  };

  for (const cf of CSER) {
    if (!opts.enabled.has(cf.k)) continue;
    push({
      key: cf.k, label: cf.lab, color: cf.col, inv: !!cf.inv, cols: cf.cols, opt: !!cf.opt,
      unit: cf.k === "cyc" ? (opts.cyclopsUnits || cf.u) : cf.u,
      band: opts.showThresholds && cf.band ? opts.thresholds[cf.band] : undefined,
    });
  }
  if (opts.showRaw) {
    for (const rf of CRAW) {
      push({ key: "raw:" + rf.col, label: rf.lab, unit: rf.u, color: "#9aa3c0", inv: false, cols: [[rf.col, 1]], opt: true });
    }
  }
  return { X, xMin, xMax, pois, metrics, skipped };
}
