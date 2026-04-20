import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// Tauri expects a fixed port and disables HMR's automatic reconnect.  Keep the
// port aligned with tauri.conf.json (devUrl).
export default defineConfig({
  plugins: [react()],
  clearScreen: false,
  server: {
    port: 5173,
    strictPort: true,
  },
  envPrefix: ["VITE_", "TAURI_"],
  build: {
    target: "es2020",
    sourcemap: true,
    outDir: "dist",
    emptyOutDir: true,
  },
});
