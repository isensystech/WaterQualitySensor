import { useEffect, useMemo, useState } from "react";
import { useAuth } from "../auth/AuthProvider";
import { Calendar } from "../components/Calendar";
import { DiveChart } from "../components/DiveChart";
import { parseCsv, type ParsedCsv } from "../lib/chart";
import {
  fetchDeviceLabels, fetchDives, fetchThresholds, thresholdsForDive,
  downloadDiveCsv, diveInstant, dayKey, fmtUtc,
  type Dive, type Threshold,
} from "../lib/dives";

interface Loaded { dives: Dive[]; labels: Record<string, string>; thresholds: Threshold[] }
interface Selected { dive: Dive; csv: string; parsed: ParsedCsv }

export function DiveGraph() {
  const { session, signOut } = useAuth();
  const [data, setData] = useState<Loaded | null>(null);
  const [error, setError] = useState("");
  const [selectedDay, setSelectedDay] = useState<string | null>(null);
  const [sel, setSel] = useState<Selected | null>(null);
  const [chartErr, setChartErr] = useState("");
  const [loadingDive, setLoadingDive] = useState<string | null>(null);

  useEffect(() => {
    let alive = true;
    Promise.all([fetchDives(), fetchDeviceLabels(), fetchThresholds()])
      .then(([dives, labels, thresholds]) => { if (alive) setData({ dives, labels, thresholds }); })
      .catch((e) => { if (alive) setError(e.message ?? String(e)); });
    return () => { alive = false; };
  }, []);

  const shown = useMemo(() => {
    if (!data) return [];
    if (!selectedDay) return data.dives;
    return data.dives.filter((d) => dayKey(diveInstant(d)) === selectedDay);
  }, [data, selectedDay]);

  const openDive = async (d: Dive) => {
    setChartErr(""); setLoadingDive(d.id);
    try {
      const csv = await downloadDiveCsv(d.storage_path);
      setSel({ dive: d, csv, parsed: parseCsv(csv) });
      queueMicrotask(() =>
        document.getElementById("chartpanel")?.scrollIntoView({ behavior: "smooth", block: "start" }));
    } catch (e) {
      setChartErr(`Could not load ${d.filename}: ${(e as Error).message ?? e}`);
    } finally {
      setLoadingDive(null);
    }
  };

  return (
    <div id="app">
      <div className="bar">
        <h2>Dives</h2>
        <div>
          <span className="hint">{session?.user.email}</span>{" "}
          <button className="ghost" onClick={signOut}>Sign out</button>
        </div>
      </div>

      {error && <p className="err">{error}</p>}
      {!data && !error && <p className="hint">Loading dives…</p>}

      {data && (
        <div className="layout">
          <div className="col-cal">
            <div className="c"><Calendar dives={data.dives} selectedDay={selectedDay} onSelectDay={setSelectedDay} /></div>
          </div>
          <div className="col-list">
            <div className="c">
              {shown.length === 0 ? (
                <p className="hint">
                  {data.dives.length === 0
                    ? "No dives visible. You must be a member of a project that has dives (see bootstrap)."
                    : "No dives on the selected day."}
                </p>
              ) : (
                shown.map((d) => (
                  <button key={d.id} className={"dive" + (sel?.dive.id === d.id ? " active" : "")}
                          onClick={() => openDive(d)} disabled={loadingDive === d.id}>
                    <b>{d.label || d.filename}</b> — {data.labels[d.device_id] || d.device_id}
                    <small>
                      {fmtUtc(diveInstant(d))} · cast {d.cast_num ?? "?"}
                      {d.site ? " · " + d.site : ""}
                      {d.mission ? " · " + d.mission : ""}
                      {d.water_type ? " · " + d.water_type : ""}
                      {d.row_count != null ? " · " + d.row_count + " rows" : ""}
                      {loadingDive === d.id ? " · loading…" : ""}
                    </small>
                  </button>
                ))
              )}
            </div>
          </div>
        </div>
      )}

      <div id="chartpanel">
        {chartErr && <p className="err">{chartErr}</p>}
        {sel && data && (
          <DiveChart
            parsed={sel.parsed}
            dive={sel.dive}
            csvText={sel.csv}
            deviceLabel={data.labels[sel.dive.device_id] || sel.dive.device_id}
            thresholds={thresholdsForDive(data.thresholds, sel.dive.project_id)}
            onClose={() => setSel(null)}
          />
        )}
      </div>
    </div>
  );
}
