import { useMemo, useState, type MouseEvent } from "react";
import {
  buildChartModel, bandRects, linePath, areaPath, xPix, yPix, fmtNum, fmtT,
  Y_TICK_FRACS, yTickValue,
  CVW, CHH, CpadL, CpadT, CpW, CpH,
  type ParsedCsv, type Band, type MetricModel, type ChartModel,
} from "../lib/chart";

// SVG ids must be unique per-document, not just per-metric — "raw:P_mbar" also isn't a safe id
// fragment (colons inside url(#...) are best avoided even though most browsers tolerate them).
const gradId = (key: string) => "grad-" + key.replace(/[^a-zA-Z0-9_-]/g, "-");

// nearest sample index to a data-x (ms); X is ascending
function nearest(X: number[], x: number): number {
  let lo = 0, hi = X.length - 1;
  if (hi < 0) return 0;
  while (lo < hi) {
    const mid = (lo + hi) >> 1;
    if (X[mid] < x) lo = mid + 1; else hi = mid;
  }
  if (lo > 0 && Math.abs(X[lo - 1] - x) <= Math.abs(X[lo] - x)) return lo - 1;
  return lo;
}

// Same "how close counts as a hit" tolerance the click handler uses (below) — hovering and
// clicking should agree on which POI is being pointed at.
const POI_HIT_PX = 16;

// Index (not sample seq) of the POI nearest svgX, or null if none is within POI_HIT_PX.
function nearestPoiOrd(pois: number[], X: number[], xMin: number, xMax: number, svgX: number): number | null {
  let best = -1, bd = Infinity;
  pois.forEach((p, ord) => {
    const d = Math.abs(xPix(X[p], xMin, xMax) - svgX);
    if (d < bd) { bd = d; best = ord; }
  });
  return best >= 0 && bd < POI_HIT_PX ? best : null;
}

interface Shared {
  model: ChartModel;
  hoverIdx: number | null;
  poiTitles: Map<number, string>;
  showLabels: boolean; // POI name labels only on the top chart to avoid repetition
  showXAxis: boolean;  // time-axis ticks only on the bottom chart — the X domain is shared/synced
  hoveredPoiOrd: number | null;
  onHover: (i: number | null) => void;
  onHoverPoi: (ord: number | null) => void;
  onPoiClick: (seq: number, ordinal: number) => void;
}

// Evenly spaced elapsed-time tick labels along the bottom axis ("t+mm:ss", matching the hover
// readout's own format) — same idea as Y_TICK_FRACS, just along x instead of y.
const X_TICK_FRACS = [0, 0.25, 0.5, 0.75, 1];

function MetricChart({
  m, model, hoverIdx, poiTitles, showLabels, showXAxis, hoveredPoiOrd, onHover, onHoverPoi, onPoiClick,
}: { m: MetricModel } & Shared) {
  const { X, xMin, xMax, pois } = model;

  // The capture <rect> IS the plot area, so its bounding box maps 1:1 to [xMin,xMax].
  // Use the rect-local fraction directly — mixing in CVW/CpadL was the crosshair-lag bug.
  const locate = (e: MouseEvent<SVGRectElement>) => {
    const r = e.currentTarget.getBoundingClientRect();
    const frac = Math.max(0, Math.min(1, (e.clientX - r.left) / r.width));
    return { dataX: xMin + frac * (xMax - xMin), svgX: CpadL + frac * CpW };
  };

  const bands = bandRects(m.band, m.yMin, m.yMax, m.inv);
  const hx = hoverIdx != null ? xPix(X[hoverIdx], xMin, xMax) : null;
  const hv = hoverIdx != null ? m.full[hoverIdx] : NaN;

  return (
    <svg viewBox={`0 0 ${CVW} ${CHH}`} className="mchart">
      <defs>
        {/* Color-matched wash under each line, top-of-line down to transparent — same idea as the
            reference design's gradient area, kept per-metric-color instead of one flat color so
            it still reads which line is which at a glance (the readout/legend swatches rely on
            that same per-metric color). */}
        <linearGradient id={gradId(m.key)} x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" stopColor={m.color} stopOpacity={0.38} />
          <stop offset="100%" stopColor={m.color} stopOpacity={0} />
        </linearGradient>
      </defs>

      {bands.map((b, i) => <rect key={i} x={CpadL} y={b.y} width={CpW} height={b.h} fill={b.fill} />)}

      {/* dashed scale gridlines + value labels at 0/25/50/75/100% — was just the top/bottom two
          values before; matches the reference design's evenly-spaced labeled scale. */}
      {Y_TICK_FRACS.map((f) => {
        const y = CpadT + f * CpH;
        return (
          <g key={f}>
            <line x1={CpadL} x2={CpadL + CpW} y1={y} y2={y} stroke="#242c48" strokeWidth={0.8} strokeDasharray="3 4" />
            <text x={CpadL - 6} y={y + (f === 0 ? 4 : f === 1 ? 0 : 4)} fill="#9aa3c0" fontSize={11} textAnchor="end">
              {fmtNum(yTickValue(f, m.yMin, m.yMax, m.inv))}
            </text>
          </g>
        );
      })}
      <rect x={CpadL} y={CpadT} width={CpW} height={CpH} fill="none" stroke="#2a3252" strokeWidth={1} rx={3} />

      {/* time-axis ticks, bottom chart only (the X domain is shared/synced across the stack —
          repeating it under every metric would just be noise). */}
      {showXAxis && X_TICK_FRACS.map((f) => {
        const x = CpadL + f * CpW;
        const dataX = xMin + f * (xMax - xMin);
        return (
          <text key={f} x={x} y={CpadT + CpH + 16} fill="#9aa3c0" fontSize={11}
                textAnchor={f === 0 ? "start" : f === 1 ? "end" : "middle"}>
            t+{fmtT(dataX - xMin)}
          </text>
        );
      })}

      {/* POI vertical markers (visual only; clicks handled by the overlay). The dot itself grows
          + gets a soft halo + a light ring when the pointer is near enough to click it — the
          cursor turning into a hand wasn't enough of a cue that THIS particular point, and not
          just "somewhere on this chart", is clickable. */}
      {pois.map((p, ord) => {
        const px = xPix(X[p], xMin, xMax);
        const title = poiTitles.get(p);
        const isHovered = ord === hoveredPoiOrd;
        return (
          <g key={p} pointerEvents="none">
            <line x1={px} y1={CpadT} x2={px} y2={CpadT + CpH} stroke="#c98bff" strokeWidth={1.4} strokeDasharray="5 6" opacity={0.7} />
            {isHovered && <circle cx={px} cy={CpadT} r={10} fill="#c98bff" opacity={0.28} />}
            <circle cx={px} cy={CpadT} r={isHovered ? 6.5 : 4.5} fill="#c98bff"
                    stroke={isHovered ? "#e8eaf0" : "none"} strokeWidth={isHovered ? 1.5 : 0}
                    style={{ transition: "r 120ms ease" }} />
            {showLabels && (
              <text x={px + 6} y={CpadT + 13} fill="#c98bff" fontSize={13} fontWeight={isHovered ? 900 : 700}>
                {title ? title.slice(0, 22) : `POI ${ord + 1}`}
              </text>
            )}
          </g>
        );
      })}

      <path d={areaPath(m.drawPts, xMin, xMax, m.yMin, m.yMax, m.inv)} fill={`url(#${gradId(m.key)})`}
            stroke="none" pointerEvents="none" />
      <path d={linePath(m.drawPts, xMin, xMax, m.yMin, m.yMax, m.inv)} fill="none" stroke={m.color}
            strokeWidth={1.9} strokeLinejoin="round" strokeLinecap="round" pointerEvents="none" />

      {/* crosshair + value dot */}
      {hx != null && (
        <g pointerEvents="none">
          <line x1={hx} y1={CpadT} x2={hx} y2={CpadT + CpH} stroke="#e8eaf0" strokeWidth={1} opacity={0.5} />
          {!isNaN(hv) && <circle cx={hx} cy={yPix(hv, m.yMin, m.yMax, m.inv)} r={4} fill={m.color} stroke="#0c1020" strokeWidth={1.4} />}
        </g>
      )}

      <text x={8} y={19} fill={m.color} fontSize={15} fontWeight={700}>{m.label}{m.unit ? ` (${m.unit})` : ""}</text>

      {/* transparent capture layer: mousemove -> crosshair + POI-dot hover, click near a POI ->
          open modal. Hover and click share the same "nearest POI within POI_HIT_PX" test, via
          nearestPoiOrd, so the dot only lights up exactly where a click would actually land. */}
      <rect x={CpadL} y={CpadT} width={CpW} height={CpH} fill="transparent" style={{ cursor: pois.length ? "pointer" : "crosshair" }}
        onMouseMove={(e) => {
          const { dataX, svgX } = locate(e);
          onHover(nearest(X, dataX));
          onHoverPoi(nearestPoiOrd(pois, X, xMin, xMax, svgX));
        }}
        onMouseLeave={() => { onHover(null); onHoverPoi(null); }}
        onClick={(e) => {
          const { svgX } = locate(e);
          const ord = nearestPoiOrd(pois, X, xMin, xMax, svgX);
          if (ord != null) onPoiClick(pois[ord], ord + 1);
        }} />
    </svg>
  );
}

export function DiveCharts({
  parsed, thresholds, enabled, showRaw, showThresholds, cyclopsUnits, poiTitles, onPoiClick,
}: {
  parsed: ParsedCsv;
  thresholds: Record<string, Band>;
  enabled: Set<string>;
  showRaw: boolean;
  showThresholds: boolean;
  cyclopsUnits?: string;
  poiTitles: Map<number, string>;
  onPoiClick: (seq: number, ordinal: number) => void;
}) {
  const model = useMemo(
    () => buildChartModel(parsed, { enabled, showRaw, showThresholds, thresholds, cyclopsUnits }),
    [parsed, enabled, showRaw, showThresholds, thresholds, cyclopsUnits],
  );
  const [hoverIdx, setHoverIdx] = useState<number | null>(null);
  // Which POI (by index into model.pois) the pointer is currently near enough to click, shared
  // across all stacked metric charts so the highlighted dot stays in sync the same way the
  // crosshair does — hovering over the top chart lights up the same POI on the charts below it.
  const [hoveredPoiOrd, setHoveredPoiOrd] = useState<number | null>(null);
  const decimated = model.metrics.some((m) => m.decimated);

  if (!model.metrics.length) {
    return <p className="hint">No plottable metrics selected (or all sensors off / uncalibrated).</p>;
  }

  return (
    <div className="chartsv">
      {/* readout card — sticky above the charts so it never obstructs them */}
      <div className="readout">
        <span className="rtime">{hoverIdx != null ? "t+" + fmtT(model.X[hoverIdx] - model.xMin) : "hover a chart →"}</span>
        {model.metrics.map((m) => (
          <span key={m.key} className="rval">
            <i style={{ background: m.color }} />{m.label}:{" "}
            <b>{hoverIdx != null ? fmtNum(m.full[hoverIdx]) : "—"}</b>{m.unit ? ` ${m.unit}` : ""}
          </span>
        ))}
      </div>

      <div className="stack">
        {model.metrics.map((m, i) => (
          <MetricChart key={m.key} m={m} model={model} hoverIdx={hoverIdx} poiTitles={poiTitles}
                       showLabels={i === 0} showXAxis={i === model.metrics.length - 1} hoveredPoiOrd={hoveredPoiOrd}
                       onHover={setHoverIdx} onHoverPoi={setHoveredPoiOrd} onPoiClick={onPoiClick} />
        ))}
      </div>

      {model.skipped.length > 0 && (
        <div className="cnote">No data (sensor off or uncalibrated): {model.skipped.join(", ")}.</div>
      )}
      {decimated && <div className="cnote">Long dive — min/max decimated to fit; excursions preserved.</div>}
      <div className="clegend">
        <span><i style={{ background: "rgba(255,164,0,.18)" }} />warn</span>
        <span><i style={{ background: "rgba(255,59,48,.20)" }} />alarm</span>
        {model.pois.length > 0 && <span><i style={{ background: "#c98bff" }} />POI ({model.pois.length}) — click to annotate</span>}
      </div>
    </div>
  );
}
