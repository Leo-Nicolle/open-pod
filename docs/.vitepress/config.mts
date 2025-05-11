import { defineConfig } from "vitepress";

const fileAndStyles: Record<string, string> = {};

export default defineConfig({
  title: "Openpod",
  description: "An ESP32 mp3 player",
  base: "/open-pod/",
  head: [["link", { rel: "icon", href: "/open-pod/favicon.svg" }]],
  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: "Home", link: "/" },
      { text: "About", link: "/about" },
      { text: "Clickwheel", link: "/clickwheel" },
    ],

    sidebar: [
      // {
      //   text: "Examples",
      //   items: [
      //     { text: "Markdown Examples", link: "/markdown-examples" },
      //     { text: "Runtime API Examples", link: "/api-examples" },
      //   ],
      // },
    ],

    socialLinks: [
      { icon: "github", link: "https://github.com/Leo-Nicolle/open-pod" },
    ],
  },
  vite: {
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
