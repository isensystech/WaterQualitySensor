import { createTheme, type MantineColorsTuple } from "@mantine/core";

// Design-system foundation for the viewer. Every hex value here is lifted straight from the
// hand-rolled `index.css` dark theme (not reinvented), so migrating a component to Mantine
// changes *how* it's built, not what it looks like. New components should pull colors/spacing
// from this theme (or Mantine's own scale) instead of introducing new one-off hex values —
// that's the whole point of "one system" instead of ad hoc styling per component.

// Teal accent used throughout index.css (headings, links, active states, badges).
const brand: MantineColorsTuple = [
  "#e7fbfb",
  "#c5f3f3",
  "#9fe9e9",
  "#75dede",
  "#4cd3d3",
  "#2ec7c7",
  "#19c3c3", // <- the actual accent color used everywhere today
  "#0fa3a3",
  "#0a8484",
  "#046666",
];

// Dark surface scale, mapped from the existing palette:
//   dark.9 = page background (#0c1020, body bg today)
//   dark.7 = card / panel surface (#161b30, the ".c" background today)
//   dark.5 = default borders (#2a3252, used on inputs/cards/chips today)
//   dark.0-3 = text, from primary (#e8eaf0) down to muted (#6b7699, ".hint"/disabled)
const dark: MantineColorsTuple = [
  "#e8eaf0",
  "#c9cee0",
  "#9aa3c0",
  "#6b7699",
  "#3a4675",
  "#2a3252",
  "#1d2440",
  "#161b30",
  "#0f1326",
  "#0c1020",
];

export const theme = createTheme({
  primaryColor: "brand",
  colors: { brand, dark },
  defaultRadius: "md",
  fontFamily: "system-ui, -apple-system, Segoe UI, Roboto, sans-serif",
  black: "#0c1020",
});
