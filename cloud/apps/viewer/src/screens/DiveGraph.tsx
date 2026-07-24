import { useEffect, useMemo, useState } from "react";
import { Button, Card, Chip, Group, Paper, Stack, Switch, Text, Title } from "@mantine/core";
import { useAuth } from "../auth/AuthProvider";
import { Calendar } from "../components/Calendar";
import { DiveChart } from "../components/DiveChart";
import { Logo } from "../components/Logo";
import { MapCard } from "../components/MapCard";
import { waterEmojiFor } from "../components/WaterTypeIcon";
import { weatherEmojiFor } from "../components/WeatherIcon";
import { CSER, parseCsv, type ParsedCsv } from "../lib/chart";
import {
  fetchDeviceLabels, fetchDives, fetchThresholds, thresholdsForDive,
  downloadDiveCsv, diveInstant, dayKey, fmtUtc,
  type Dive, type Threshold,
} from "../lib/dives";

interface Loaded { dives: Dive[]; labels: Record<string, string>; thresholds: Threshold[] }
interface Selected { dive: Dive; csv: string; parsed: ParsedCsv }

// TEMPORARY — icon legend for Kate's review (see DiveGraph's return below). Runs one
// representative keyword per bucket through the real waterEmojiFor/weatherEmojiFor classifiers
// (rather than hardcoding the emoji here) so the legend can't drift out of sync with the actual
// matching logic in WaterTypeIcon.tsx / WeatherIcon.tsx. Remove this block + its render site once
// Kate's done reviewing.
// Hidden for now (flip to true to bring it back) — Kate asked to hide without deleting it.
const SHOW_ICON_LEGEND = false;

const WATER_LEGEND: [string, string][] = [
  ["ocean", "Ocean / sea / salt / marine"],
  ["estuary", "Estuary / brackish / delta / tidal"],
  ["lake", "Lake / reservoir"],
  ["river", "River / stream / creek"],
  ["pool", "Pool"],
  ["???", "Anything else (unrecognized)"],
].map(([kw, label]) => [waterEmojiFor(kw), label]);

const WEATHER_LEGEND: [string, string][] = [
  ["tornado", "Tornado"],
  ["thunderstorm", "Thunderstorm"],
  ["lightning", "Lightning (no storm/rain)"],
  ["snow", "Snow"],
  ["shower", "Sun shower"],
  ["rain", "Rain / drizzle"],
  ["fog", "Fog / mist / haze"],
  ["wind", "Wind / breezy"],
  ["mostly cloudy", "Mostly cloudy"],
  ["overcast", "Cloudy / overcast"],
  ["mostly sunny", "Mostly sunny"],
  ["clear", "Clear / sunny"],
  ["partly cloudy", "Partly cloudy (also the default)"],
].map(([kw, label]) => [weatherEmojiFor(kw), label]);

// Page chrome migrated to Mantine (header, map/calendar/list/chart/controls cards). Calendar,
// MapCard and DiveChart keep their own internals/classes for now — they're just placed inside
// Mantine Paper containers here instead of the old `.c`/`.dive` divs.
//
// Responsive placement of the five cards (map, calendar, controls, list, charts) lives in the
// ".dashboard" grid in index.css, with three tiers:
//   - narrow (<720px, phones): one column, stacked map / calendar / controls / list / charts
//   - medium (720-1179px, "almost square" windows): map full-width on top, calendar+list side
//     by side below it, controls full-width, then charts full-width at the bottom
//   - wide (>=1180px, a maximized monitor): calendar, map and the display-settings controls
//     stack as one unit to the left of the charts, with the dive list on the far right
//
// The metric-visibility controls (enabled/showRaw/showThresholds) live here, as state, and get
// passed down to DiveChart as props — they used to be local state inside DiveChart, but Kate
// wanted the controls card moved out to sit alongside the calendar/map, so the state moved up
// to whichever component is the parent of both.
export function DiveGraph() {
  const { session, signOut } = useAuth();
  const [data, setData] = useState<Loaded | null>(null);
  const [error, setError] = useState("");
  const [selectedDay, setSelectedDay] = useState<string | null>(null);
  const [sel, setSel] = useState<Selected | null>(null);
  const [chartErr, setChartErr] = useState("");
  const [loadingDive, setLoadingDive] = useState<string | null>(null);

  const [enabled, setEnabled] = useState<Set<string>>(() => new Set(CSER.map((m) => m.k)));
  const [showRaw, setShowRaw] = useState(false);
  const [showThresholds, setShowThresholds] = useState(true);

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

  const selThresholds = useMemo(
    () => (sel && data ? thresholdsForDive(data.thresholds, sel.dive.project_id) : {}),
    [sel, data],
  );
  const hasThresholds = Object.keys(selThresholds).length > 0;

  const toggleMetric = (k: string) =>
    setEnabled((s) => { const n = new Set(s); n.has(k) ? n.delete(k) : n.add(k); return n; });

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
    <Stack gap="md" id="app">
      <Group justify="space-between" align="center">
        <Group gap={6} align="center">
          <Logo size={42} />
          <Group gap={12} align="baseline">
            <Text fw={700} c="brand" size="xl">PlanetWerx</Text>
            <Text size="md" c="dimmed">Dive Log Viewer</Text>
          </Group>
        </Group>
        <Group gap="sm" align="center">
          <Text size="sm" c="dimmed" style={{ lineHeight: 1 }}>{session?.user.email}</Text>
          <Button variant="default" size="xs" onClick={signOut}>Sign out</Button>
        </Group>
      </Group>

      {error && <Text c="red">{error}</Text>}
      {!data && !error && <Text c="dimmed">Loading dives…</Text>}

      {data && (
        <div className="dashboard">
          {/* .d-leftstack only matters at the wide tier (>=1180px), where it becomes a real
              flex column so calendar+map+controls stack as one unit with no gap between them.
              At narrower tiers it's `display: contents` (see index.css) — invisible to layout —
              so .d-cal/.d-map/.d-controls go back to being independent grid areas. */}
          <div className="d-leftstack">
            <div className="d-cal">
              <Paper radius={12} p="sm" bg="dark.7">
                <Calendar dives={data.dives} selectedDay={selectedDay} onSelectDay={setSelectedDay} />
              </Paper>
            </div>

            <div className="d-map">
              <Paper radius={12} p="sm" bg="dark.7">
                <Title order={5} c="brand" mb="xs">🗺️ Sites</Title>
                <MapCard dives={data.dives} onSelect={openDive} />
              </Paper>
            </div>

            {sel && (
              <div className="d-controls">
                <Paper radius={12} p="sm" bg="dark.7">
                  <Title order={5} c="brand" mb="xs">⚙️ Display</Title>
                  {/* Mantine Chip instead of checkbox+label: no visible checkbox, active/
                      inactive is just a bright-vs-dimmed color difference, and it's a good bit
                      more compact than a checkbox + colored swatch + text per row. */}
                  <Group gap={6}>
                    {CSER.map((m) => (
                      <Chip key={m.k} checked={enabled.has(m.k)} onChange={() => toggleMetric(m.k)}
                            color={m.col} variant="filled" size="xs">
                        {m.lab}
                      </Chip>
                    ))}
                    <Chip checked={showThresholds} disabled={!hasThresholds}
                          onChange={setShowThresholds} variant="filled" size="xs">
                      Threshold bands{!hasThresholds ? " (none set)" : ""}
                    </Chip>
                  </Group>
                  {/* Raw diagnostic channels is a different kind of parameter than the chips
                      above — those pick WHICH metrics/bands to show, this is a mode switch for
                      the raw/uncalibrated sensor feed, so it gets its own full-width row (name +
                      Switch pushed to opposite ends) with real breathing room above it, instead of
                      blending in as one more item in the chip row. */}
                  <Group justify="space-between" wrap="nowrap" mt="sm">
                    <Text size="xs" c="dimmed">Raw diagnostic channels</Text>
                    <Switch checked={showRaw} onChange={(e) => setShowRaw(e.currentTarget.checked)} size="xs" />
                  </Group>
                </Paper>
              </div>
            )}
          </div>

          <div className="d-list">
            <Paper radius={12} p="md" bg="dark.7">
              {shown.length === 0 ? (
                <Text c="dimmed">
                  {data.dives.length === 0
                    ? "No dives visible. You must be a member of a project that has dives (see bootstrap)."
                    : "No dives on the selected day."}
                </Text>
              ) : (
                <Stack gap="xs">
                  {shown.map((d) => (
                    <Card
                      key={d.id}
                      component="button"
                      onClick={() => openDive(d)}
                      disabled={loadingDive === d.id}
                      withBorder
                      radius="md"
                      p="sm"
                      bg="dark.6"
                      style={{
                        width: "100%",
                        textAlign: "left",
                        cursor: "pointer",
                        borderColor: sel?.dive.id === d.id ? "var(--mantine-color-brand-6)" : undefined,
                      }}
                    >
                      <Text fw={700} c="brand" size="sm">
                        {d.label || d.filename} — {data.labels[d.device_id] || d.device_id}
                      </Text>
                      <Text size="xs" c="dimmed" mt={4}>
                        {fmtUtc(diveInstant(d))} · cast {d.cast_num ?? "?"}
                        {d.site ? " · " + d.site : ""}
                        {d.mission ? " · " + d.mission : ""}
                        {d.water_type ? " · " + d.water_type : ""}
                        {d.row_count != null ? " · " + d.row_count + " rows" : ""}
                        {loadingDive === d.id ? " · loading…" : ""}
                      </Text>
                    </Card>
                  ))}
                </Stack>
              )}
            </Paper>
          </div>

          <div className="d-charts" id="chartpanel">
            {chartErr && <Text c="red">{chartErr}</Text>}
            {sel && (
              <DiveChart
                parsed={sel.parsed}
                dive={sel.dive}
                csvText={sel.csv}
                deviceLabel={data.labels[sel.dive.device_id] || sel.dive.device_id}
                thresholds={selThresholds}
                enabled={enabled}
                showRaw={showRaw}
                showThresholds={showThresholds}
                onClose={() => setSel(null)}
              />
            )}
          </div>
        </div>
      )}

      {/* TEMPORARY — icon legend for review; delete this Paper (and the two LEGEND consts
          above) once Kate's done checking the water/weather icon sets.
          Currently hidden (SHOW_ICON_LEGEND = false above) at Kate's request — kept in the
          code, just not rendered, so it's a one-line flip to bring back rather than redone
          from scratch. */}
      {SHOW_ICON_LEGEND && (
      <Paper radius={12} p="md" bg="dark.7">
        <Title order={5} c="brand" mb="xs">🧪 Icon legend (temporary)</Title>
        <Text size="xs" c="dimmed" mb="sm">Every icon the Water and Weather stat-cards can currently show, with the condition each one means.</Text>
        <Stack gap="md">
          <div>
            <Text size="xs" fw={700} c="dimmed" tt="uppercase" mb={6}>Water</Text>
            <Group gap="lg">
              {WATER_LEGEND.map(([emoji, label]) => (
                <Group key={label} gap={6} wrap="nowrap">
                  <Text size="lg" style={{ lineHeight: 1 }}>{emoji}</Text>
                  <Text size="xs" c="dimmed">{label}</Text>
                </Group>
              ))}
            </Group>
          </div>
          <div>
            <Text size="xs" fw={700} c="dimmed" tt="uppercase" mb={6}>Weather</Text>
            <Group gap="lg">
              {WEATHER_LEGEND.map(([emoji, label]) => (
                <Group key={label} gap={6} wrap="nowrap">
                  <Text size="lg" style={{ lineHeight: 1 }}>{emoji}</Text>
                  <Text size="xs" c="dimmed">{label}</Text>
                </Group>
              ))}
            </Group>
          </div>
        </Stack>
      </Paper>
      )}
    </Stack>
  );
}
