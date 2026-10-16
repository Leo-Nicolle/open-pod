import { fileURLToPath } from "node:url";
import { defineConfig } from "vitepress";

const fileAndStyles: Record<string, string> = {};

export default defineConfig({
  title: "Openpod",
  description: "An open-source iPod-style music player",
  base: "/open-pod/",
  head: [
    ["link", { rel: "icon", href: "/open-pod/favicon.svg" }],
    ["link", { rel: "preconnect", href: "https://fonts.googleapis.com" }],
    ["link", { rel: "preconnect", href: "https://fonts.gstatic.com", crossorigin: "" }],
    [
      "link",
      {
        rel: "stylesheet",
        href: "https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@400;500;600;700&family=IBM+Plex+Mono:wght@400;500;600&display=swap",
      },
    ],
  ],
  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    logo: "/openpod_logo.svg",
    siteTitle: "Openpod",
    nav: [
      { text: "Manual", link: "/manual/", activeMatch: "^/manual/" },
      {
        text: "Tools",
        activeMatch: "^/tools/",
        items: [
          { text: "OpenPod Sync", link: "/tools/sync" },
          { text: "Click wheel designer", link: "/tools/clickwheel" },
          { text: "Pinout", link: "/tools/pinout" },
          { text: "UI builder", link: "/tools/ui-builder" },
          { text: "Benchmarks", link: "/tools/bench" },
          { text: "Live bench", link: "/tools/bench-live" },
          { text: "Compare bench reports", link: "/tools/bench-compare" },
        ],
      },
      { text: "Blog", link: "/blog/", activeMatch: "^/blog/" },
      { text: "About", link: "/about" },
    ],

    sidebar: {
      "/manual/": [
        {
          text: "User manual",
          items: [{ text: "Overview", link: "/manual/" }],
        },
      ],
      "/tools/": [
        {
          text: "Tools",
          items: [
            { text: "Overview", link: "/tools/" },
            { text: "OpenPod Sync", link: "/tools/sync" },
            { text: "Click wheel designer", link: "/tools/clickwheel" },
            { text: "UI builder", link: "/tools/ui-builder" },
            { text: "Benchmarks", link: "/tools/bench" },
            { text: "Live bench", link: "/tools/bench-live" },
            { text: "Compare bench reports", link: "/tools/bench-compare" },
          ],
        },
        {
          text: "Pinout",
          items: [
            { text: "Interactive pinout", link: "/tools/pinout" },
            { text: "LQFP100 table (final board)", link: "/tools/pinout-table-lqfp100" },
            { text: "LQFP64 table (Nucleo)", link: "/tools/pinout-table" },
          ],
        },
      ],
      "/blog/": [
        {
          text: "Blog",
          items: [{ text: "All articles", link: "/blog/" }],
        },
        {
          text: "2026",
          items: [
            { text: "Bluetooth module setup", link: "/blog/11-10" },
            { text: "Smooth FLAC", link: "/blog/09-10" },
            { text: "Displaying covers", link: "/blog/20-09" },
          ],
        },
        {
          text: "2025",
          items: [
            { text: "Indexing and search", link: "/blog/01-07" },
            { text: "Fast track scrolling", link: "/blog/14-06" },
            { text: "Scrolling and transitions", link: "/blog/03-06" },
            { text: "Vibecoding the UI", link: "/blog/01-06" },
            { text: "Working with the screen", link: "/blog/31-05" },
            { text: "Designing a click wheel", link: "/blog/15-04" },
          ],
        },
      ],
    },

    socialLinks: [
      { icon: "github", link: "https://github.com/Leo-Nicolle/open-pod" },
    ],
  },
  vite: {
    // The bench pages share the runner's code with software/bench/.
    resolve: {
      alias: { "@bench": fileURLToPath(new URL("../../software/bench", import.meta.url)) },
    },
    server: { fs: { allow: [fileURLToPath(new URL("../..", import.meta.url))] } },
    ssr: {
      noExternal: ["naive-ui", "date-fns", "vueuc"],
    },
  },
  postRender(context) {
    const styleRegex = /<css-render-style>((.|\s)+)<\/css-render-style>/;
    const vitepressPathRegex = /<vitepress-path>(.+)<\/vitepress-path>/;
    const style = styleRegex.exec(context.content)?.[1];
    const vitepressPath = vitepressPathRegex.exec(context.content)?.[1];
    if (vitepressPath && style) {
      fileAndStyles[vitepressPath] = style;
    }
    context.content = context.content.replace(styleRegex, "");
    context.content = context.content.replace(vitepressPathRegex, "");
  },
  transformHtml(code, id) {
    const html = id.split("/").pop();
    if (!html) return;
    const style = fileAndStyles[`/${html}`];
    if (style) {
      return code.replace(/<\/head>/, `${style}</head>`);
    }
  },
});
