import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import { viteSingleFile } from "vite-plugin-singlefile";

// Single-file build: inline JS+CSS into one self-contained dist/index.html so the app can be
// served by anything (Supabase Edge Function, Cloudflare Pages, a bare file) with no asset-path
// or SPA-rewrite config. The bundle is small enough that losing code-splitting doesn't matter.
// https://vite.dev/config/
export default defineConfig({
  plugins: [react(), viteSingleFile()],
  build: { outDir: "dist", sourcemap: false, assetsInlineLimit: 100000000 },
});
