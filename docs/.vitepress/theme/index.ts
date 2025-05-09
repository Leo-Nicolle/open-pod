// https://vitepress.dev/guide/custom-theme
import Theme from "vitepress/theme-without-fonts";
import { EnhanceAppContext } from "vitepress";
import Clickwheel from "./components/clickwheel.vue";

export default {
  extends: Theme,
  enhanceApp({ app }: EnhanceAppContext) {
    app.component("Clickwheel", Clickwheel);
  },
};
