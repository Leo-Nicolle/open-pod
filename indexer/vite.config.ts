import { defineConfig } from "vite";
import { resolve } from "path";
import { name } from "./package.json";
export default defineConfig(({ command, mode }) => {
  const isDev = command === "serve";

  // if (isDev) {
  //   // Development configuration - runs demo
  //   return {
  //     root: "demo",
  //     server: {
  //       port: 3000,
  //       open: true,
  //     },
  //     resolve: {
  //       alias: {
  //         // Alias your lib so demo can import it
  //         "@/lib": resolve(__dirname, "src"),
  //       },
  //     },
  //   };
  // } else {
  // Build configuration - builds the library
  return {
    build: {
      lib: {
        entry: resolve(__dirname, "src/main.ts"),
        name,
        formats: ["es", "cjs"],
        fileName: (format) => `index.${format === "es" ? "mjs" : "js"}`,
      },
      rollupOptions: {
        // Externalize dependencies that shouldn't be bundled
        external: ["fs", "path", "util", "events", "stream", "crypto"],
        output: {
          globals: {
            // Define globals for UMD build if needed
          },
        },
      },
      target: "node14",
      outDir: "dist",
      emptyOutDir: true,
    },
  };
  // }
});
