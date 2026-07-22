import type { ReactNode } from "react";
import type { Dive } from "../lib/dives";
import { fmtUtc } from "../lib/dives";

// Dive metadata as pictogram chips (emoji, no icon-lib dependency) with `title` tooltips.
// Sourced from the typed dive row (not the raw '#' header) so values are clean and structured.
function Chip({ icon, tip, children }: { icon: string; tip: string; children: ReactNode }) {
  return (
    <span className="metachip" title={tip}>
      <span className="mi" aria-hidden>{icon}</span>{children}
    </span>
  );
}

export function DiveMeta({ dive }: { dive: Dive }) {
  const cals: [string, boolean | null][] = [
    ["pH", dive.cal_ph], ["EC", dive.cal_ec], ["ORP", dive.cal_orp], ["Cyclops", dive.cal_cyc],
  ];
  const sensors: [string, boolean | null][] = [
    ["POET", dive.poet_en], ["BAR30", dive.bar30_en], ["Celsius", dive.cels_en], ["Cyclops", dive.cyc_en],
  ];
  const hasGps = dive.lat != null && dive.lon != null;

  return (
    <div className="metawrap">
      <div className="metarow">
        {dive.cast_num != null && <Chip icon="🔢" tip="Cast number">Cast {dive.cast_num}</Chip>}
        {dive.mission && <Chip icon="🎯" tip="Mission">{dive.mission}</Chip>}
        {dive.operator && <Chip icon="👤" tip="Operator">{dive.operator}</Chip>}
        {dive.site && <Chip icon="📍" tip="Site">{dive.site}</Chip>}
        {dive.water_type && <Chip icon="🌊" tip="Water type">{dive.water_type}</Chip>}
        {(dive.weather || dive.air_temp_c != null) && (
          <Chip icon="⛅" tip="Weather / air temperature">
            {dive.weather || "—"}{dive.air_temp_c != null ? ` · ${dive.air_temp_c}°C air` : ""}
          </Chip>
        )}
        {hasGps && (
          <Chip icon="🗺️" tip="GPS where measurements were taken">
            {dive.lat!.toFixed(4)}, {dive.lon!.toFixed(4)}
          </Chip>
        )}
        {(dive.started_at || dive.utc_start) && (
          <Chip icon="🕐" tip={`Start time (${dive.time_source || "time source unknown"})`}>
            {fmtUtc(dive.started_at ?? dive.utc_start)}
          </Chip>
        )}
        {dive.row_count != null && <Chip icon="📈" tip="Logged sample rows">{dive.row_count} rows</Chip>}
        {dive.notes && <Chip icon="📝" tip="Notes">{dive.notes}</Chip>}
      </div>
      <div className="metarow">
        <span className="metalabel" title="Which channels were calibrated for this dive">cal:</span>
        {cals.map(([k, v]) => (
          <span key={k} className={"calchip " + (v ? "on" : "off")} title={`${k} ${v ? "calibrated" : "not calibrated"}`}>
            {v ? "✓" : "✗"} {k}
          </span>
        ))}
        <span className="metalabel" title="Which sensors were enabled">sensors:</span>
        {sensors.map(([k, v]) => (
          <span key={k} className={"calchip " + (v ? "on" : "off")} title={`${k} ${v ? "enabled" : "off / absent"}`}>
            {v ? "●" : "○"} {k}
          </span>
        ))}
      </div>
    </div>
  );
}
