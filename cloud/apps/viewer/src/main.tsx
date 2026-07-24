import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { HashRouter } from "react-router-dom";
import { MantineProvider } from "@mantine/core";
import App from "./App";
import { AuthProvider } from "./auth/AuthProvider";
import { theme } from "./theme";
// Mantine's stylesheet first, our own hand-rolled stylesheet second — so index.css (still
// styling everything not yet migrated to Mantine) always wins on anything it already sets,
// e.g. body background/color. Nothing here removes index.css; components move over one at a
// time, and until they do they keep using the classes they already had.
import "@mantine/core/styles.css";
import "./index.css";

// HashRouter (not BrowserRouter): routes live in the URL hash, so the app is a single static
// index.html that works from ANY path with no server rewrite — Supabase Edge Function host,
// Cloudflare Pages, or a plain file. (Trade-off: `#` in URLs, fine for an internal tool.)
createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <MantineProvider theme={theme} defaultColorScheme="dark">
      <HashRouter>
        <AuthProvider>
          <App />
        </AuthProvider>
      </HashRouter>
    </MantineProvider>
  </StrictMode>,
);
