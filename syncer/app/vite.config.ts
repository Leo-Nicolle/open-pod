import vue from '@vitejs/plugin-vue';
import { defineConfig } from 'vite';

// Tauri expects a fixed port (1420: away from VitePress's 5173, used by docs/) and doesn't need the dev server's screen clearing.
export default defineConfig({
  plugins: [vue()],
  clearScreen: false,
  server: { port: 1420, strictPort: true },
  envPrefix: ['VITE_', 'TAURI_ENV_'],
  build: { target: 'es2022' },
});
