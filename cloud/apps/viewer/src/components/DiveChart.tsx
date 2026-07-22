import { useCallback, useEffect, useMemo, useState } from "react";
import { useAuth } from "../auth/AuthProvider";
import { DiveCharts } from "./DiveCharts";
import { DiveMeta } from "./DiveMeta";
import { AnnotationModal } from "./AnnotationModal";
import { CSER, fmtT, type Band, type ParsedCsv } from "../lib/chart";
import { fetchDiveAnnotations, groupBySeq } from "../lib/annotations";
import type { Dive } from "../lib/dives";

// Dive graph container: metadata pictograms + metric-visibility controls + the interactive
// single-column charts (synced crosshair, hover readout, POI names) + the POI annotation modal.
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
  const { session } = useAuth();
  const authorId = session?.user.id ?? "";

  const [enabled, setEnabled] = useState<Set<string>>(() => new Set(CSER.map((m) => m.k)));
  const [showRaw, setShowRaw] = useState(false);
  const [showThresholds, setShowThresholds] = useState(true);
  const [poiTitles, setPoiTitles] = useState<Map<number, string>>(new Map());
  const [modal, setModal] = useState<{ seq: number; ordinal: number; timeLabel: string } | null>(null);

  const hasThresholds = Object.keys(thresholds).length > 0;

  const loadAnn = useCallback(async () => {
    try {
      const g = groupBySeq(await fetchDiveAnnotations(dive.id));
      const m = new Map<number, string>();
      g.forEach((v, seq) => { if (v.note?.title) m.set(seq, v.note.title); });
      setPoiTitles(m);
    } catch { /* annotations are optional; ignore load errors */ }
  }, [dive.id]);
  useEffect(() => { loadAnn(); }, [loadAnn]);

  const msArr = useMemo(() => {
    const i = parsed.idx["ms"];
    return parsed.rows.map((r) => (i != null ? r[i] : NaN));
  }, [parsed]);

  const openPoi = (seq: number, ordinal: number) => {
    const t = !isNaN(msArr[seq]) && !isNaN(msArr[0]) ? fmtT(msArr[seq] - msArr[0]) : String(seq);
    setModal({ seq, ordinal, timeLabel: t });
  };

  const toggle = (k: string) =>
    setEnabled((s) => { const n = new Set(s); n.has(k) ? n.delete(k) : n.add(k); return n; });

  const dlCsv = () => {
    const a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob([csvText], { type: "text/csv" }));
    a.download = dive.filename;
    a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 5000);
  };

  return (
    <div className="c">
      <div className="chdr">
        <b>{dive.label || dive.filename} — {deviceLabel}</b>
        <span>
          <button className="xbtn" onClick={dlCsv}>download CSV</button>{" "}
          <button className="xbtn" onClick={onClose}>close</button>
        </span>
      </div>

      <DiveMeta dive={dive} />

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

      {!parsed.rows.length ? (
        <p className="hint">No data rows in this dive.</p>
      ) : (
        <DiveCharts
          parsed={parsed}
          thresholds={thresholds}
          enabled={enabled}
          showRaw={showRaw}
          showThresholds={showThresholds && hasThresholds}
          cyclopsUnits={dive.cyclops_units ?? undefined}
          poiTitles={poiTitles}
          onPoiClick={openPoi}
        />
      )}

      {modal && authorId && (
        <AnnotationModal
          diveId={dive.id} seq={modal.seq} ordinal={modal.ordinal} timeLabel={modal.timeLabel}
          authorId={authorId} onClose={() => setModal(null)} onSaved={loadAnn}
        />
      )}
    </div>
  );
}
