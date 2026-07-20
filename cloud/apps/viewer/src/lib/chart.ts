// chart.ts — SVG small-multiples renderer, PORTED from firmware/src/portal_page.h
// (Hard Rule 6: same look, same NaN-gap handling, same min/max-preserving decimation, same
// threshold bands). Kept as string-building SVG — no chart lib, no CDN — so the viewer chart
// is byte-for-byte the shape the logger's own portal draws. Keep in sync with portal_page.h.

const CVW = 336, CHH = 120, CpadL = 44, CpadR = 8, CpadT = 18, CpadB = 14;
const CpW = CVW - CpadL - CpadR, CpH = CHH - CpadT - CpadB;

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
  cols: [string, number][]; // preference list: [column, scale]
  lab: string;
  u: string;
  col: string;
  inv?: number;
  band?: string; // threshold key
  opt?: number;  // optional metric -> no "no data" note when absent
}

// default metric charts; temp prefers Celsius -> BAR30 -> POET(milli-C). band = threshold key
export const CSER: MetricDef[] = [
  { k: "depth", cols: [["depth_m", 1]], lab: "Depth", u: "m", col: "#58a6ff", inv: 1, band: "depth" },
  { k: "temp", cols: [["cels_T_C", 1], ["bar30T_C", 1], ["poetT_mC", 0.001]], lab: "Temp", u: "°C", col: "#ff8c5a", band: "temp" },
  { k: "ph", cols: [["pH", 1]], lab: "pH", u: "", col: "#c98bff", band: "ph" },
  { k: "ec", cols: [["EC_mScm", 1]], lab: "Conductivity", u: "mS/cm", col: "#5dd6c0", band: "ec" },
  { k: "sal", cols: [["sal_PSU", 1]], lab: "Salinity", u: "PSU", col: "#7fd6ff", band: "sal" },
  { k: "orp", cols: [["ORP_Eh_mV", 1]], lab: "ORP", u: "mV", col: "#e0c060", band: "orp" },
  { k: "cyc", cols: [["cyc_conc", 1]], lab: "Fluorometry", u: "", col: "#6ee07a", band: "cyc", opt: 1 },
];

// diagnostic raw channels, hidden behind the "show raw" toggle
export const CRAW: { col: string; lab: string; u: string }[] = [
  { col: "P_mbar", lab: "Pressure", u: "mbar" },
  { col: "ugs_uV", lab: "pH raw (Ugs)", u: "µV" },
  { col: "orp_uV", lab: "ORP raw", u: "µV" },
  { col: "ec_nA", lab: "EC current", u: "nA" },
  { col: "ec_uV", lab: "EC voltage", u: "µV" },
  { col: "cyc_V", lab: "Cyclops raw", u: "V" },
];

export const esc = (s: unknown): string =>
  String(s == null ? "" : s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
const Nn = (x: string | undefined): number => (x == null || x === "" ? NaN : +x);
const clmp = (v: number, a: number, b: number) => (v < a ? a : v > b ? b : v);
export const fmtNum = (v: number): string => {
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

// first column with any data wins (for temp's preference list); empty cell -> NaN gap
function seriesPts(P: ParsedCsv, X: number[], cols: [string, number][]): Pt[] | null {
  for (const [name, sc] of cols) {
    const i = P.idx[name];
    if (i == null) continue;
    const pts: Pt[] = [];
    let any = false;
    for (let r = 0; r < P.rows.length; r++) {
      let v = P.rows[r][i];
      if (!isNaN(v)) { any = true; v *= sc; }
      pts.push([X[r], v]);
    }
    if (any) return pts;
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

function linePath(pts: Pt[], X: (x: number) => number, Y: (y: number) => number): string {
  let d = "", pen = false;
  for (const p of pts) {
    const y = p[1];
    if (isNaN(y)) { pen = false; continue; }
    d += (pen ? "L" : "M") + X(p[0]).toFixed(1) + " " + Y(y).toFixed(1) + " ";
    pen = true;
  }
  return d;
}

function bandRect(loV: number, hiV: number, Y: (y: number) => number, yMin: number, yMax: number, fill: string): string {
  loV = clmp(loV, yMin, yMax); hiV = clmp(hiV, yMin, yMax);
  if (loV >= hiV) return "";
  const y1 = Y(hiV), y2 = Y(loV), top = Math.min(y1, y2), h = Math.abs(y2 - y1);
  if (h < 0.5) return "";
  return `<rect x="${CpadL}" y="${top.toFixed(1)}" width="${CpW}" height="${h.toFixed(1)}" fill="${fill}" />`;
}

// shade alarm (red) + warn (amber) zones; NaN/undefined bound = that bound disabled
function bandSvg(b: Band | undefined, Y: (y: number) => number, yMin: number, yMax: number): string {
  if (!b) return "";
  const wlo = b.wlo ?? NaN, whi = b.whi ?? NaN, alo = b.alo ?? NaN, ahi = b.ahi ?? NaN;
  const R = "rgba(255,59,48,.20)", A = "rgba(255,164,0,.18)";
  let s = "";
  if (!isNaN(ahi)) s += bandRect(ahi, yMax, Y, yMin, yMax, R);
  if (!isNaN(whi)) s += bandRect(whi, isNaN(ahi) ? yMax : ahi, Y, yMin, yMax, A);
  if (!isNaN(wlo)) s += bandRect(isNaN(alo) ? yMin : alo, wlo, Y, yMin, yMax, A);
  if (!isNaN(alo)) s += bandRect(yMin, alo, Y, yMin, yMax, R);
  return s;
}

interface ChartOpts { pts: Pt[]; lab: string; u: string; col: string; inv?: number; band?: Band }

function miniChart(o: ChartOpts, xMin: number, xMax: number, pois: number[]): string {
  let yMin = Infinity, yMax = -Infinity;
  for (const p of o.pts) { const y = p[1]; if (!isNaN(y)) { if (y < yMin) yMin = y; if (y > yMax) yMax = y; } }
  if (yMin === Infinity) { yMin = 0; yMax = 1; }
  if (yMin === yMax) { yMin -= 1; yMax += 1; }
  const pd = (yMax - yMin) * 0.08; yMin -= pd; yMax += pd;
  const rng = (yMax - yMin) || 1, xr = (xMax - xMin) || 1;
  const Y = (v: number) => (o.inv ? CpadT + ((v - yMin) / rng) * CpH : CpadT + ((yMax - v) / rng) * CpH);
  const X = (x: number) => CpadL + ((x - xMin) / xr) * CpW;
  let s = `<svg viewBox="0 0 ${CVW} ${CHH}">`;
  s += `<rect x="${CpadL}" y="${CpadT}" width="${CpW}" height="${CpH}" fill="none" stroke="#2a3252" />`;
  s += bandSvg(o.band, Y, yMin, yMax);
  for (const px0 of pois) {
    const px = X(px0).toFixed(1);
    s += `<line x1="${px}" y1="${CpadT}" x2="${px}" y2="${CpadT + CpH}" stroke="#c98bff" stroke-dasharray="3 3" opacity="0.7" />`;
  }
  s += `<path d="${linePath(o.pts, X, Y)}" fill="none" stroke="${o.col}" stroke-width="1.5" stroke-linejoin="round" stroke-linecap="round" />`;
  s += `<text x="4" y="13" fill="${o.col}" font-size="11" font-weight="700">${esc(o.lab)}${o.u ? " (" + esc(o.u) + ")" : ""}</text>`;
  s += `<text x="${CpadL - 3}" y="${CpadT + 4}" fill="#9aa3c0" font-size="9" text-anchor="end">${fmtNum(o.inv ? yMin : yMax)}</text>`;
  s += `<text x="${CpadL - 3}" y="${CpadT + CpH}" fill="#9aa3c0" font-size="9" text-anchor="end">${fmtNum(o.inv ? yMax : yMin)}</text>`;
  return s + "</svg>";
}

export interface RenderOpts {
  enabled: Set<string>;
  showRaw: boolean;
  showThresholds: boolean;
  thresholds: Record<string, Band>;
  cyclopsUnits?: string;
}
export interface DiveRender {
  charts: { key: string; label: string; svg: string }[];
  pois: number[];
  xMin: number;
  xMax: number;
  skipped: string[];
  decimated: boolean;
}

// drawCharts() equivalent: builds the per-metric SVGs for the enabled metrics.
export function renderDive(D: ParsedCsv, opts: RenderOpts): DiveRender {
  const mi = D.idx["ms"], pi = D.idx["poi"];
  const X = new Array<number>(D.rows.length), pois: number[] = [];
  for (let r = 0; r < D.rows.length; r++) {
    const xv = mi != null ? D.rows[r][mi] : NaN;
    X[r] = isNaN(xv) ? r : xv;
    if (pi != null && D.rows[r][pi] === 1) pois.push(X[r]);
  }
  let xMin = X[0] ?? 0, xMax = X[X.length - 1] ?? 1;
  if (!(xMax > xMin)) xMax = xMin + 1;

  const charts: DiveRender["charts"] = [], skipped: string[] = [];
  let decimated = false;
  for (const cf of CSER) {
    if (!opts.enabled.has(cf.k)) continue;
    const pts = seriesPts(D, X, cf.cols);
    if (!pts) { if (!cf.opt) skipped.push(cf.lab); continue; }
    const dd = decimate(pts, CpW);
    if (dd.dec) decimated = true;
    const band = opts.showThresholds && cf.band ? opts.thresholds[cf.band] : undefined;
    const u = cf.k === "cyc" ? (opts.cyclopsUnits || cf.u) : cf.u;
    charts.push({ key: cf.k, label: cf.lab, svg: miniChart({ pts: dd.pts, lab: cf.lab, u, col: cf.col, inv: cf.inv, band }, xMin, xMax, pois) });
  }
  if (opts.showRaw) {
    for (const rf of CRAW) {
      const rpts = seriesPts(D, X, [[rf.col, 1]]);
      if (!rpts) continue;
      const rd = decimate(rpts, CpW);
      if (rd.dec) decimated = true;
      charts.push({ key: "raw:" + rf.col, label: rf.lab, svg: miniChart({ pts: rd.pts, lab: rf.lab, u: rf.u, col: "#9aa3c0" }, xMin, xMax, pois) });
    }
  }
  return { charts, pois, xMin, xMax, skipped, decimated };
}
