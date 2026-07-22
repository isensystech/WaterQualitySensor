import { useEffect, useRef } from "react";
import maplibregl, { type StyleSpecification } from "maplibre-gl";
import "maplibre-gl/dist/maplibre-gl.css";
import type { Dive } from "../lib/dives";
import { fmtUtc, diveInstant } from "../lib/dives";

// MapLibre GL JS (Phase 6, DECISION #4) with free OpenStreetMap raster tiles — no token.
// One marker per dive that has GPS; click a marker to open that dive's graph.
// inline raster style (valid at runtime; object literals widen the discriminant strings, so
// cast rather than fight the StyleSpecification union).
const OSM_STYLE = {
  version: 8,
  sources: { osm: { type: "raster", tiles: ["https://tile.openstreetmap.org/{z}/{x}/{y}.png"], tileSize: 256, attribution: "© OpenStreetMap contributors" } },
  layers: [{ id: "osm", type: "raster", source: "osm" }],
} as unknown as StyleSpecification;

const esc = (s: unknown) => String(s ?? "").replace(/[&<>]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;" }[c]!));

export function MapCard({ dives, onSelect }: { dives: Dive[]; onSelect: (d: Dive) => void }) {
  const box = useRef<HTMLDivElement>(null);
  const sel = useRef(onSelect);
  sel.current = onSelect; // keep latest callback without re-creating the map

  const geo = dives.filter((d) => d.lat != null && d.lon != null);

  useEffect(() => {
    if (!box.current || geo.length === 0) return;
    const map = new maplibregl.Map({
      container: box.current, style: OSM_STYLE,
      center: [geo[0].lon!, geo[0].lat!], zoom: 9, attributionControl: { compact: true },
    });
    map.addControl(new maplibregl.NavigationControl(), "top-right");
    const bounds = new maplibregl.LngLatBounds();
    for (const d of geo) {
      const marker = new maplibregl.Marker({ color: "#19c3c3" }).setLngLat([d.lon!, d.lat!]).addTo(map);
      marker.setPopup(new maplibregl.Popup({ offset: 24, closeButton: false }).setHTML(
        `<div class="mappop"><b>${esc(d.label || d.filename)}</b><br>${esc(d.site || "")} ${esc(fmtUtc(diveInstant(d)))}<br><i>click marker to open</i></div>`,
      ));
      const el = marker.getElement();
      el.style.cursor = "pointer";
      el.addEventListener("click", () => sel.current(d));
      bounds.extend([d.lon!, d.lat!]);
    }
    if (geo.length > 1) map.fitBounds(bounds, { padding: 44, maxZoom: 12, duration: 0 });
    return () => map.remove();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [dives]);

  if (geo.length === 0) {
    return <p className="hint">No dives have GPS yet — the map shows a marker per geolocated dive.</p>;
  }
  return (
    <>
      <div ref={box} className="mapbox" />
      <p className="hint" style={{ marginTop: 6 }}>{geo.length} geolocated dive{geo.length > 1 ? "s" : ""} · click a marker to open its graph.</p>
    </>
  );
}
