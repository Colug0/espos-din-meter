// SPDX-FileCopyrightText: 2026 Thomas Christoffel
// SPDX-License-Identifier: Apache-2.0
//
// espOS's UI with the Pages tab added (docs/ui.md in espOS, "Pages from a
// firmware"). The espOS sources are compiled straight from the submodule.
import { defineConfig } from "vite";
import preact from "@preact/preset-vite";

// `npm run dev` talks to a device: ESPOS_API=http://<device> npm run dev
const target = process.env["ESPOS_API"] ?? "http://127.0.0.1:8484";

// espOS strips `crossorigin` for the same reason: iOS's captive-portal
// webview blocks a module script fetched in CORS mode from a device that
// sends no CORS headers, and the setup portal shows a black page.
function stripCrossorigin() {
  return {
    name: "strip-crossorigin",
    enforce: "post" as const,
    transformIndexHtml(html: string) {
      return html.replace(/\s+crossorigin(=("|')[^"']*\2)?/g, "");
    },
  };
}

export default defineConfig({
  plugins: [preact(), stripCrossorigin()],
  resolve: {
    // The espOS sources import preact from outside this package; one copy
    // only, or hooks break.
    dedupe: ["preact"],
  },
  build: {
    outDir: "dist",
    emptyOutDir: true,
    sourcemap: false,
    target: "es2020",
    cssCodeSplit: false,
  },
  server: {
    port: 5173,
    fs: { allow: [".."] },
    proxy: { "/api": { target, changeOrigin: true, ws: false } },
  },
});
