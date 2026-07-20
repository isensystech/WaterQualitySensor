import { useMemo, useState } from "react";
import type { Dive } from "../lib/dives";
import { dayKey, diveInstant } from "../lib/dives";

const MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
const DOW = ["S", "M", "T", "W", "T", "F", "S"];

// Days with logs are marked, badge = # logs that day (Phase 5). All dates are UTC to match
// how dives are timestamped. Clicking a day filters the dive list; clicking it again clears.
export function Calendar({
  dives, selectedDay, onSelectDay,
}: { dives: Dive[]; selectedDay: string | null; onSelectDay: (d: string | null) => void }) {
  const counts = useMemo(() => {
    const m: Record<string, number> = {};
    for (const d of dives) {
      const k = dayKey(diveInstant(d));
      if (k) m[k] = (m[k] ?? 0) + 1;
    }
    return m;
  }, [dives]);

  // default view = month of the most recent dive (or now if none)
  const initial = useMemo(() => {
    const keys = Object.keys(counts).sort();
    const latest = keys.length ? keys[keys.length - 1] : new Date().toISOString().slice(0, 10);
    return { y: +latest.slice(0, 4), m: +latest.slice(5, 7) - 1 };
  }, [counts]);

  const [ym, setYm] = useState(initial);
  // keep the view aligned to data if the dive set changes underneath us
  const view = ym.y ? ym : initial;

  const first = new Date(Date.UTC(view.y, view.m, 1));
  const startDow = first.getUTCDay();
  const daysInMonth = new Date(Date.UTC(view.y, view.m + 1, 0)).getUTCDate();

  const cells: (number | null)[] = [];
  for (let i = 0; i < startDow; i++) cells.push(null);
  for (let d = 1; d <= daysInMonth; d++) cells.push(d);

  const step = (delta: number) => {
    const nm = view.m + delta;
    setYm({ y: view.y + Math.floor(nm / 12), m: ((nm % 12) + 12) % 12 });
  };
  const keyOf = (d: number) =>
    `${view.y}-${String(view.m + 1).padStart(2, "0")}-${String(d).padStart(2, "0")}`;

  return (
    <div className="cal">
      <div className="calhdr">
        <button className="xbtn" onClick={() => step(-1)} aria-label="Previous month">‹</button>
        <b>{MONTHS[view.m]} {view.y}</b>
        <button className="xbtn" onClick={() => step(1)} aria-label="Next month">›</button>
      </div>
      <div className="calgrid caldow">{DOW.map((d, i) => <span key={i}>{d}</span>)}</div>
      <div className="calgrid">
        {cells.map((d, i) => {
          if (d == null) return <span key={i} className="calcell empty" />;
          const k = keyOf(d);
          const n = counts[k] ?? 0;
          const cls = "calcell" + (n ? " has" : "") + (k === selectedDay ? " sel" : "");
          return (
            <button key={i} className={cls} disabled={!n}
                    onClick={() => onSelectDay(k === selectedDay ? null : k)}>
              <span className="cald">{d}</span>
              {n > 0 && <span className="calbadge">{n}</span>}
            </button>
          );
        })}
      </div>
      {selectedDay && (
        <button className="ghost calclear" onClick={() => onSelectDay(null)}>
          Showing {selectedDay} · show all
        </button>
      )}
    </div>
  );
}
