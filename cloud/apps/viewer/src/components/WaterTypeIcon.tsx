// Icon for the "Water" stat card's water_type value. Kate didn't want hand-drawn glyphs here
// (unlike WeatherIcon/ClockIcon) — just existing, standard emoji, one per value the portal's
// "Water type" dropdown currently offers (firmware/src/portal_page.h: ocean, estuary, lake,
// river, pool). 🌊 is the original icon this whole card used before any of this — kept as-is for
// ocean. The DB column is free text with no CHECK constraint, so it can (and does, on real dives
// like "Salt") hold values outside those five — keyword-matched here the same way weatherKindFor
// already handles weather's free text, instead of requiring an exact match against the dropdown
// list. Only a value that matches nothing below falls back to a plain droplet.
export function waterEmojiFor(waterType: string | null | undefined): string {
  const w = (waterType ?? "").trim().toLowerCase();
  if (/estuar|brackish|delta|tidal/.test(w)) return "🛶";
  if (/lake|reservoir/.test(w)) return "🏞️";
  if (/river|stream|creek/.test(w)) return "🚣";
  if (/pool/.test(w)) return "🏊";
  if (/ocean|sea|salt|marine/.test(w)) return "🌊";
  return "💧";
}
