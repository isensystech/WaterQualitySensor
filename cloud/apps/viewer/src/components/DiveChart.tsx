import { useMemo, useState } from "react";
import { CSER, renderDive, fmtT, type Band, type ParsedCsv } from "../lib/chart";
import type { Dive } from "../lib/dives";

// Screen 2 core: multi-metric view with per-metric vertical scaling (each mini-chart
// auto-scales to its own range), metric visibility toggles, and a threshold-marker toggle.
// Rendering is the ported portal_page.h SVG builder (Hard Rule 6).
export function DiveChart({
  parsed, dive, deviceLabel, csvText, thresholds, onClose,
}: {
  parsed: ParsedCsv;
  dive: Dive;
  deviceLabel: string;
  csvText: string;
  thresholds: Record<string, Band>;
  onClose: () => void;
}) {
  const [enabled, setEnabled] = useState<Set<string>>(() => new Set(CSER.map((m) => m.k)));
  const [showRaw, setShowRaw] = useState(false);
  const [showThresholds, setShowThresholds] = useState(true);

  const hasThresholds = Object.keys(thresholds).length > 0;

  const render = useMemo(
    () => renderDive(parsed, {
      enabled, showRaw,
      showThresholds: showThresholds && hasThresholds,
      thresholds,
      cyclopsUnits: dive.cyclops_units ?? undefined,
    }),
    [parsed, enabled, showRaw, showThresholds, hasThresholds, thresholds, dive.cyclops_units],
  );

  const toggle = (k: string) =>
    setEnabled((s) => { const n = new Set(s); n.has(k) ? n.delete(k) : n.add(k); return n; });

  const dlCsv = () => {
    const a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob([csvText], { type: "text/csv" }));
    a.download = dive.filename;
    a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 5000);
  };

  const { charts, pois, xMin, xMax, skipped, decimated } = render;

  return (
    <div className="c">
      <div className="chdr">
        <b>{dive.filename} — {deviceLabel}</b>
        <span>
          <button className="xbtn" onClick={dlCsv}>download CSV</button>{" "}
          <button className="xbtn" onClick={onClose}>close</button>
        </span>
      </div>

      {parsed.meta.length > 0 && (
        <div className="cmeta">{parsed.meta.map((m, i) => <div key={i}>{m}</div>)}</div>
      )}

      <div className="controls">
        {CSER.map((m) => (
          <label key={m.k} className={"chip" + (enabled.has(m.k) ? " on" : "")}>
            <input type="checkbox" checked={enabled.has(m.k)} onChange={() => toggle(m.k)} />
            <i style={{ background: m.col }} />{m.lab}
          </label>
        ))}
        <label className="chip">
          <input type="checkbox" checked={showThresholds} disabled={!hasThresholds}
                 onChange={(e) => setShowThresholds(e.target.checked)} />
          Threshold bands{!hasThresholds ? " (none set)" : ""}
        </label>
        <label className="chip">
          <input type="checkbox" checked={showRaw} onChange={(e) => setShowRaw(e.target.checked)} />
          Raw diagnostic channels
        </label>
      </div>

      {skipped.length > 0 && (
        <div className="cnote">No data (sensor off or uncalibrated): {skipped.join(", ")}.</div>
      )}
      {decimated && (
        <div className="cnote">Long dive — min/max decimated to fit; excursions preserved.</div>
      )}

      {charts.length > 0 ? (
        <>
          <div className="charts">
            {charts.map((c) => (
              <div key={c.key} dangerouslySetInnerHTML={{ __html: c.svg }} />
            ))}
          </div>
          <div className="xax">
            <span>0:00</span><span>{fmtT((xMax - xMin) / 2)}</span><span>{fmtT(xMax - xMin)}</span>
          </div>
          <div className="clegend">
            <span><i style={{ background: "rgba(255,164,0,.18)" }} />warn</span>
            <span><i style={{ background: "rgba(255,59,48,.20)" }} />alarm</span>
            {pois.length > 0 && <span><i style={{ background: "#c98bff" }} />POI ({pois.length})</span>}
          </div>
        </>
      ) : (
        <p className="hint">No plottable metrics selected (or all sensors off / uncalibrated).</p>
      )}
    </div>
  );
}
