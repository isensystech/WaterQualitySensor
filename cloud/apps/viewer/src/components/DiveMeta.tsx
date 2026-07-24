import type { ReactNode } from "react";
import type { Dive } from "../lib/dives";
import { fmtUtc } from "../lib/dives";
import { weatherEmojiFor } from "./WeatherIcon";
import { ClockIcon } from "./ClockIcon";
import { waterEmojiFor } from "./WaterTypeIcon";

// One icon-led metadata line — parts joined with " | " under a single leading icon, e.g.
// "🎯 Cast 1 | Cloud smoke test". Returns null (renders nothing) when there are no parts, so
// callers can drop the result straight into JSX without a separate length check.
function metaLine(icon: string, tip: string, parts: string[]): ReactNode {
  if (parts.length === 0) return null;
  return (
    <div className="metaline" title={tip}>
      <span className="mi" aria-hidden>{icon}</span>{parts.join(" | ")}
    </div>
  );
}

// DiveInfo: identity byline (cast/mission on one line, operator on its own, site/GPS on a third),
// the CAL/SENSORS status grid, and the free-text note — everything except the four square stat
// cards. Rendered inside the "big square" tile, under the page-level title (see DiveChart.tsx's
// .chdr). Three stacked lines rather than one dot-separated run — Kate wanted cast+mission,
// operator, and site+GPS visually grouped as three separate facts, not one long chain.
export function DiveInfo({ dive }: { dive: Dive }) {
  const cals: [string, boolean | null][] = [
    ["pH", dive.cal_ph], ["EC", dive.cal_ec], ["ORP", dive.cal_orp], ["Cyclops", dive.cal_cyc],
  ];
  const sensors: [string, boolean | null][] = [
    ["POET", dive.poet_en], ["BAR30", dive.bar30_en], ["Celsius", dive.cels_en], ["Cyclops", dive.cyc_en],
  ];
  const hasGps = dive.lat != null && dive.lon != null;

  const missionParts: string[] = [];
  if (dive.cast_num != null) missionParts.push(`Cast ${dive.cast_num}`);
  if (dive.mission) missionParts.push(dive.mission);

  const siteParts: string[] = [];
  if (dive.site) siteParts.push(dive.site);
  if (hasGps) siteParts.push(`${dive.lat!.toFixed(4)}, ${dive.lon!.toFixed(4)}`);

  const line1 = metaLine("🎯", "Cast / mission", missionParts);
  const line2 = dive.operator ? metaLine("👤", "Operator", [dive.operator]) : null;
  const line3 = metaLine("📍", "Site / GPS", siteParts);

  return (
    <>
      {(line1 || line2 || line3) && (
        <div className="metalines">
          {line1}
          {line2}
          {line3}
        </div>
      )}

      <div className="statusgrid">
        <div className="statusrow">
          <span className="statuslabel">SENSORS</span>
          {sensors.map(([k, v]) => (
            <span key={k} className={"statuscell " + (v ? "on" : "off")} title={`${k} ${v ? "enabled" : "off / absent"}`}>
              {v ? "●" : "○"} {k}
            </span>
          ))}
        </div>
        <div className="statusrow">
          <span className="statuslabel">CALIBRATION</span>
          {cals.map(([k, v]) => (
            <span key={k} className={"statuscell " + (v ? "on" : "off")} title={`${k} ${v ? "calibrated" : "not calibrated"}`}>
              {v ? "✓" : "✗"} {k}
            </span>
          ))}
        </div>
      </div>

      {dive.notes && <p className="metanote">{dive.notes}</p>}
    </>
  );
}

// A square "stat card" — title + round icon avatar, big bold value, short caption. Modeled after
// dashboard KPI tiles, minus the trend arrow/percentage: these are static facts about one
// already-completed dive, there's no "previous period" to compare against.
function StatCard({ icon, title, value, caption, tip }: {
  icon: ReactNode; title: string; value: ReactNode; caption?: string; tip: string;
}) {
  return (
    <div className="statcard" title={tip}>
      <div className="statcard-head">
        <span className="statcard-title">{title}</span>
        <span className="statcard-icon" aria-hidden>{icon}</span>
      </div>
      <div>
        <div className="statcard-value">{value}</div>
        {caption && <div className="statcard-caption">{caption}</div>}
      </div>
    </div>
  );
}

// Which of the four optional stat cards a dive actually has — used both to render them (below)
// and to tell the settings grid how many card ROWS to reserve (see diveStatCardRows). Kept as one
// shared list so the two can never drift apart.
function statCardFlags(dive: Dive): boolean[] {
  const hasWeather = dive.weather != null || dive.air_temp_c != null;
  const hasTime = Boolean(dive.started_at || dive.utc_start);
  return [hasWeather, hasTime, dive.row_count != null, Boolean(dive.water_type)];
}

// Number of 2-per-row lines the stat cards will occupy for this dive (1 for 1-2 cards, 2 for 3-4).
// Purely derived from already-loaded dive fields — no DOM measurement — so DiveChart.tsx can set
// `.dcsettings-grid`'s `grid-template-rows` explicitly and correctly to match. This turned out to
// be load-bearing, not cosmetic: without an explicit grid-template-rows, `.dc-bigbox`'s
// `grid-row: 1 / -1` resolves `-1` against the EXPLICIT grid only (CSS Grid spec) — with none
// declared, that collapses to a span of just 1 row, so the big box silently stopped covering the
// second row of cards, and the next auto-placed card fell into the now-"free" first column instead
// of the second, throwing off both the height match and the square shape. Verified in a standalone
// Chromium repro before/after adding this — see chat history for the measurements.
export function diveStatCardRows(dive: Dive): number {
  const count = statCardFlags(dive).filter(Boolean).length;
  return Math.max(1, Math.ceil(count / 2));
}

// DiveStatCards: the four square tiles (weather, when, how much data, water type) — split out
// from DiveInfo so they can sit as their own block next to the "big square" (see DiveChart.tsx).
export function DiveStatCards({ dive }: { dive: Dive }) {
  const [hasWeather, hasTime] = statCardFlags(dive);

  // fmtUtc renders "YYYY-MM-DD HH:MM UTC" — split so the time is the big glanceable value and
  // the date/timezone become the caption underneath.
  let timeValue: ReactNode = "—";
  let timeCaption = "";
  // Hour/minute for the clock icon's hands — read straight off the same Date fmtUtc formats
  // (and in the same UTC terms it uses), rather than re-parsing the "HH:MM" string, so the hands
  // can't drift out of sync with the displayed time. Null when there's no valid timestamp; the
  // "Logged at" card just isn't rendered in that case (see hasTime below).
  let timeHM: [number, number] | null = null;
  if (hasTime) {
    const full = fmtUtc(dive.started_at ?? dive.utc_start);
    const [datePart, clockPart, tz] = full.split(" ");
    if (clockPart) { timeValue = clockPart; timeCaption = `${datePart} ${tz ?? ""}`.trim(); }
    else { timeValue = full; }

    const d = new Date(dive.started_at ?? dive.utc_start ?? "");
    if (!Number.isNaN(d.getTime())) timeHM = [d.getUTCHours(), d.getUTCMinutes()];
  }

  return (
    <div className="statcards">
      {hasWeather && (
        <StatCard
          icon={weatherEmojiFor(dive.weather)} title="Weather"
          value={dive.air_temp_c != null ? `${dive.air_temp_c}°C` : "—"}
          caption={dive.weather ?? undefined}
          tip={dive.weather ?? "Weather / air temperature"}
        />
      )}
      {hasTime && (
        <StatCard
          icon={timeHM ? <ClockIcon hour={timeHM[0]} minute={timeHM[1]} size={22} /> : "🕐"}
          title="Logged at" value={timeValue} caption={timeCaption}
          tip={`Start time (${dive.time_source || "time source unknown"})`}
        />
      )}
      {dive.row_count != null && (
        <StatCard icon="📈" title="Rows" value={dive.row_count} caption="logged samples" tip="Logged sample rows" />
      )}
      {dive.water_type && (
        <StatCard icon={waterEmojiFor(dive.water_type)} title="Water" value={dive.water_type} tip="Water type" />
      )}
    </div>
  );
}
