// Icon for the "Weather" stat card's weather value. Was originally hand-drawn SVG geometry (to
// avoid pulling weather.com's own copyrighted icon graphics into the app), but Kate wants this
// to match the Water card's approach instead — existing, standard emoji, not custom artwork.
// Same keyword classification as before (weather.com's own condition vocabulary is used only as
// a text-classification scheme, not their graphics), now mapped straight to an emoji per kind.
// Order matters below: more specific multi-word phrases (e.g. "mostly cloudy") are checked before
// the single-keyword buckets they'd otherwise also match (e.g. plain "cloudy").
export function weatherEmojiFor(weather: string | null | undefined): string {
  const w = (weather ?? "").toLowerCase();
  if (/tornado|twister/.test(w)) return "🌪️";
  if (/thunder|storm/.test(w)) return "⛈️";
  if (/lightning/.test(w)) return "🌩️";
  if (/snow|sleet|flurr/.test(w)) return "🌨️";
  if (/shower/.test(w)) return "🌦️";        // checked before rain: "rain shower" reads as a shower
  if (/rain|drizzle/.test(w)) return "🌧️";
  if (/fog|mist|haze/.test(w)) return "🌫️";
  if (/wind|breez|gale/.test(w)) return "💨";
  if (/mostly cloudy/.test(w)) return "🌥️";
  // "Partly cloudy" contains "cloudy", so it has to be caught here too — otherwise the generic
  // overcast/cloudy/clouds check right below would grab it first and show ☁️ instead of ⛅.
  if (/partly cloudy/.test(w)) return "⛅";
  if (/overcast|cloudy|clouds/.test(w)) return "☁️";
  if (/mostly sunny/.test(w)) return "🌤️";
  if (/clear|sun/.test(w)) return "☀️";
  if (/partly|mostly/.test(w)) return "⛅";
  return "⛅";
}
