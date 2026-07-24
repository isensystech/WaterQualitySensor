import { useCallback, useEffect, useMemo, useState } from "react";
import { useAuth } from "../auth/AuthProvider";
import { DiveCharts } from "./DiveCharts";
import { DiveInfo, DiveStatCards, diveStatCardRows } from "./DiveMeta";
import { AnnotationModal } from "./AnnotationModal";
import { fmtT, type Band, type ParsedCsv } from "../lib/chart";
import { fetchDiveAnnotations, groupBySeq } from "../lib/annotations";
import type { Dive } from "../lib/dives";

// Dive graph container: metadata pictograms + the interactive single-column charts (synced
// crosshair, hover readout, POI names) + the POI annotation modal. Metric-visibility controls
// (which metrics/thresholds/raw channels to show) used to live here too, but now live in their
// own card in the left stack (see DiveGraph.tsx) — enabled/showRaw/showThresholds are passed in
// as props instead of being local state.
export function DiveChart({
  parsed, dive, deviceLabel, csvText, thresholds, enabled, showRaw, showThresholds, onClose,
}: {
  parsed: ParsedCsv;
  dive: Dive;
  deviceLabel: string;
  csvText: string;
  thresholds: Record<string, Band>;
  enabled: Set<string>;
  showRaw: boolean;
  showThresholds: boolean;
  onClose: () => void;
}) {
  const { session } = useAuth();
  const authorId = session?.user.id ?? "";

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

  const dlCsv = () => {
    const a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob([csvText], { type: "text/csv" }));
    a.download = dive.filename;
    a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 5000);
  };

  return (
    <div className="dchart">
      {/* Back to one card (undoing the earlier two-card split) — but the settings portion keeps
          its 5-tile mosaic: one big square (facts + CAL/SENSORS + note) next to a 2x2 block of
          four small squares (weather, when, rows, water type), followed by the charts + hover
          legend underneath, all inside the same "c" panel. */}
      <div className="c dpanel">
        {/* Just the title + download button up here, above the mosaic — the identity byline
            (cast/mission/operator) moved back into .dc-bigbox, see DiveInfo in DiveMeta.tsx.
            Close now floats in the panel's own corner (.dpanel > .xclose, no background) rather
            than sitting paired with "download CSV" — it dismisses the WHOLE panel, not just this
            row, so it shouldn't read as a third row-level action next to a content button. */}
        <div className="chdr">
          <div className="chdrmain">
            <b>{dive.label || dive.filename} — {deviceLabel}</b>
          </div>
          <span className="chdrbtns">
            <button className="xbtn" onClick={dlCsv}>download CSV</button>
          </span>
        </div>
        <button className="xclose" onClick={onClose} aria-label="Close" title="Close" />

        {/* grid-template-rows is set explicitly (from data, not measurement) because
            .dc-bigbox's `grid-row: 1 / -1` needs a real explicit grid to span against —
            see the comment on diveStatCardRows in DiveMeta.tsx for why. */}
        <div className="dcsettings-grid" style={{ gridTemplateRows: `repeat(${diveStatCardRows(dive)}, 1fr)` }}>
          <div className="dc-bigbox">
            <DiveInfo dive={dive} />
          </div>

          <div className="dc-statwrap">
            <DiveStatCards dive={dive} />
          </div>
        </div>

        <div className="dc-chartarea">
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
        </div>
      </div>

      {modal && authorId && (
        <AnnotationModal
          diveId={dive.id} seq={modal.seq} ordinal={modal.ordinal} timeLabel={modal.timeLabel}
          authorId={authorId} onClose={() => setModal(null)} onSaved={loadAnn}
        />
      )}
    </div>
  );
}
