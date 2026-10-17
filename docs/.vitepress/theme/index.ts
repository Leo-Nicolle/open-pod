import { EnhanceAppContext, useRoute, withBase } from "vitepress";
import Clickwheel from "./components/clickwheel.vue";
import Video from "./components/video.vue";
import IndexCalculator from "./components/index-calculator.vue";
import UIBuilder from "./components/ui-builder/UIBuilder.vue";
import PinoutViewer from "./components/pinmap/PinoutViewer.vue";
import BenchReport from "./components/BenchReport.vue";
import BenchLive from "./components/bench/BenchLive.vue";
import BenchCompare from "./components/bench/BenchCompare.vue";
import ScrollConfig from "./components/bench/ScrollConfig.vue";
import MemoryLayout from "./components/memory/MemoryLayout.vue";
import PinoutLayout from "./layouts/PinoutLayout.vue";
import SyncLayout from "./layouts/SyncLayout.vue";
import { setup } from "@css-render/vue3-ssr";
import { NConfigProvider } from "naive-ui";
import DefaultTheme from "vitepress/theme";
import "./style.css";
import { defineComponent, h, inject } from "vue";
import { onMounted, watch } from 'vue'
const { Layout } = DefaultTheme;

const CssRenderStyle = defineComponent({
  setup() {
    const collect = inject("css-render-collect");
    return {
      style: collect(),
    };
  },
  render() {
    return h("css-render-style", {
      innerHTML: this.style,
    });
  },
});

const VitepressPath = defineComponent({
  setup() {
    const route = useRoute();
    return () => {
      return h("vitepress-path", null, [route.path]);
    };
  },
});

const NaiveUIProvider = defineComponent({
  render() {
    return h(
      NConfigProvider,
      { abstract: true, inlineThemeDisabled: true },
      {
        default: () => [
          h(Layout, null, { default: this.$slots.default?.() }),
          import.meta.env.SSR ? [h(CssRenderStyle), h(VitepressPath)] : null,
        ],
      }
    );
  },
});

export default {
  extends: DefaultTheme,
  Layout: NaiveUIProvider,
  enhanceApp({ app }: EnhanceAppContext) {
    app.component("Clickwheel", Clickwheel);
    app.component("Video", Video);
    app.component("IndexCalculator", IndexCalculator);
    app.component("UIBuilder", UIBuilder);
    app.component("PinoutViewer", PinoutViewer);
    app.component("BenchReport", BenchReport);
    app.component("BenchLive", BenchLive);
    app.component("BenchCompare", BenchCompare);
    app.component("ScrollConfig", ScrollConfig);
    app.component("MemoryLayout", MemoryLayout);
    app.component("PinoutLayout", PinoutLayout);
    app.component("SyncLayout", SyncLayout);

    if (import.meta.env.SSR) {
      const { collect } = setup(app);
      app.provide("css-render-collect", collect);
    }
  },

  setup() {
    const route = useRoute()

    const updateFavicon = async () => {
      // Prevent running during server-side build steps
      try {
        const response = await fetch(withBase('/opentone_logo.svg'));
        const svgText = await response.text();

        // Parse the SVG text with DOMParser
        const parser = new DOMParser();
        const doc = parser.parseFromString(svgText, 'image/svg+xml');
        const svgElement = doc.querySelector('svg');

        if (svgElement) {
          // Modify the viewBox dynamically via JS
          svgElement.setAttribute('viewBox', '70 0 1 60');
          // Serialize back to string and convert to Data URL
          const serializer = new XMLSerializer();
          const updatedSvgString = serializer.serializeToString(svgElement);
          const dataUrl = `data:image/svg+xml,${encodeURIComponent(updatedSvgString)}`;
          let faviconLink = document.querySelector("link[rel*='icon']") || document.createElement('link');
          faviconLink.rel = 'icon';
          faviconLink.type = 'image/svg+xml';
          faviconLink.href = dataUrl;
          document.head.appendChild(faviconLink);
        }
      } catch (error) {
        console.error('Failed to load or modify SVG favicon:', error);
      }
    }

    onMounted(() => {
      updateFavicon() // Run on initial page load
    })

    // Watch for route changes if you want it to recalculate per page
    watch(() => route.path, () => {
      updateFavicon()
    })
  }
};
