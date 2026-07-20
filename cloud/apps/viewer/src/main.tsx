import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { HashRouter } from "react-router-dom";
import App from "./App";
import { AuthProvider } from "./auth/AuthProvider";
import "./index.css";

// HashRouter (not BrowserRouter): routes live in the URL hash, so the app is a single static
// index.html that works from ANY path with no server rewrite — Supabase Edge Function host,
// Cloudflare Pages, or a plain file. (Trade-off: `#` in URLs, fine for an internal tool.)
createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <HashRouter>
      <AuthProvider>
        <App />
      </AuthProvider>
    </HashRouter>
  </StrictMode>,
);
