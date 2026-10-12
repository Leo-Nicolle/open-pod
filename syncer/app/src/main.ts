import '@fontsource/space-grotesk/400.css';
import '@fontsource/space-grotesk/500.css';
import '@fontsource/space-grotesk/600.css';
import '@fontsource/space-grotesk/700.css';
import '@fontsource/ibm-plex-mono/400.css';
import '@fontsource/ibm-plex-mono/500.css';
import './styles.css';
import { createApp } from 'vue';
import App from './App.vue';
import { start, store, type Screen } from './store';

const q = new URLSearchParams(location.search);
const theme = q.get('theme');
if (theme === 'light' || theme === 'dark') document.documentElement.dataset.theme = theme;

if (q.has('selftest')) void import('./selftest').then((m) => m.selftest());
else start();
// Preview a screen directly (sample data in a browser): ?screen=whatsnew
const screen = q.get('screen');
if (screen) store.screen = screen as Screen;
createApp(App, { embed: q.has('embed') }).mount('#app');
