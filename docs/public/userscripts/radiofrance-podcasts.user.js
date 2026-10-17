// ==UserScript==
// @name         opentone — Radio France podcast downloader
// @namespace    https://github.com/Leo-Nicolle/opentone
// @version      0.3.0
// @description  Adds a panel to Radio France podcast pages to download episodes into a folder (e.g. ~/Podcasts), ready for opentone Sync.
// @author       opentone
// @homepageURL  https://opentone.eu/tools/
// @downloadURL  https://opentone.eu/userscripts/radiofrance-podcasts.user.js
// @updateURL    https://opentone.eu/userscripts/radiofrance-podcasts.user.js
// @match        https://www.radiofrance.fr/*/podcasts/*
// @match        https://radiofrance.fr/*/podcasts/*
// @run-at       document-idle
// @grant        GM_xmlhttpRequest
// @grant        GM_download
// @connect      radiofrance.fr
// @connect      radiofrance-podcast.net
// @connect      proxycast.radiofrance.fr
// @connect      media.radiofrance-podcast.net
// @noframes
// ==/UserScript==

// Adds a floating panel on any Radio France podcast page. The page's <head>
// carries the show's RSS feed (<link rel="alternate" type="application/rss+xml">),
// so the episode list comes from the feed, not from scraping the SPA.
//
// Chrome/Edge (File System Access API) write straight into a folder you pick
// once (default ~/Podcasts), as <Station>/<Show>/<YYYY-MM-DD> - <Title>.<ext>.
// Firefox/Safari can't write a chosen folder, so they fall back to a normal
// download. The feed is CORS-open so `fetch` reads it, but the audio enclosure
// host (proxycast) 302s without a CORS header, so episodes are pulled with
// GM_xmlhttpRequest, which bypasses CORS and follows the redirect.
//
// The files keep Radio France's own tags; normalisation (station/series tree)
// happens offline, in the opentone tagger (see notes/podcasts.md).

(function () {
  "use strict";

  const PANEL_ID = "opentone-rf-panel";
  const STYLE_ID = "opentone-rf-style";
  const DIR_KEY = "opentone-rf-podcasts-dir";
  const FALLBACK_STATION = "Radio France";
  const MAX_NAME = 140;

  const STATIONS = {
    franceculture: "France Culture",
    franceinter: "France Inter",
    franceinfo: "France Info",
    francemusique: "France Musique",
    francebleu: "France Bleu",
    mouv: "Mouv'",
    ici: "Ici",
  };

  const LOGO_SVG =
    `<svg version="1.2" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 99 122" width="99" height="122"><style> .oprf-s0 { fill: #47143a } .oprf-s1 { fill: #161228 } .oprf-s2 { fill: #e2282c } .oprf-s3 { fill: #6e7274 } .oprf-s4 { fill: #e8e0cc } .oprf-s5 { fill: #972d6f } .oprf-s6 { fill: #60a534 } .oprf-s7 { fill: #96dd42 } .oprf-s8 { fill: #cb5297 } .oprf-s9 { fill: #6f1d55 } .oprf-s10 { fill: #c89224 } .oprf-s11 { fill: #7a4e20 } .oprf-s12 { fill: #2c7038 } .oprf-s13 { fill: #303a47 } </style><path fill-rule="evenodd" class="oprf-s0" d="m24 84l24-4 2 10-8 5-4 6-6-2-6 5-3-4 5-8h-8z"/><path fill-rule="evenodd" class="oprf-s1" d="m32 85l16-5 2 10-10 2z"/><path fill-rule="evenodd" class="oprf-s2" d="m23 91l5 2-11 12-6 2z"/><path fill-rule="evenodd" class="oprf-s3" d="m47 94h6l2 7-1 5h-4l-1-6z"/><path fill-rule="evenodd" class="oprf-s3" d="m43 108l7-4h6l9 4-3 3-7-2-3 4-4-3-4 1z"/><path fill-rule="evenodd" class="oprf-s4" d="m50 104h6l-3 4z"/><path fill-rule="evenodd" class="oprf-s3" d="m65 92h6l2 7-5 1z"/><path fill-rule="evenodd" class="oprf-s3" d="m64 101l10-2 8 2 5 5-9-2-5 3-7-2z"/><path fill-rule="evenodd" class="oprf-s4" d="m74 99l8 2-5 2z"/><path fill-rule="evenodd" class="oprf-s1" d="m54 39l9 3 25-1 5 7 1 10-2 8-4 7-8 6-4 5-8 4h-10l-5-7 2-9 1-16z"/><path fill-rule="evenodd" class="oprf-s5" d="m89.5 43l3.5 5 1 10h-4l-0.5-10z"/><path fill-rule="evenodd" class="oprf-s0" d="m90 58h4l-2 8-4 7 1.5-7z"/><path fill-rule="evenodd" class="oprf-s6" d="m63 43l7-3 4 5v5l-7-4z"/><path fill-rule="evenodd" class="oprf-s7" d="m70 40h12l5 2v4l-9 5-4-1v-5z"/><path fill-rule="evenodd" class="oprf-s5" d="m59 45.5l7 2.5 8 4v7.5l-15-2z"/><path fill-rule="evenodd" class="oprf-s8" d="m74 52l4 1 9-5 1 9.5-14 2z"/><path fill-rule="evenodd" class="oprf-s9" d="m58 59.5l16 2v6.5l-16-2z"/><path fill-rule="evenodd" class="oprf-s5" d="m74 61.5l14-2-1 6.5-13 2z"/><path fill-rule="evenodd" class="oprf-s0" d="m57 68l17 2v7l-18-2.5z"/><path fill-rule="evenodd" class="oprf-s9" d="m74 70l12-2-4 6-6 4-2-1z"/><path fill-rule="evenodd" class="oprf-s0" d="m56 76.5l18 2.5 2 1-2 4-7 3-8-1-5-5z"/><path fill-rule="evenodd" class="oprf-s2" d="m62.7 68l-3.2 3.2-3.2-3.2 3.2-3.2z"/><path fill-rule="evenodd" class="oprf-s10" d="m61 68l-1.5 1.5-1.5-1.5 1.5-1.5z"/><path fill-rule="evenodd" class="oprf-s2" d="m85.3 70l-2.8 2.8-2.8-2.8 2.8-2.8z"/><path fill-rule="evenodd" class="oprf-s10" d="m83.8 70l-1.3 1.3-1.3-1.3 1.3-1.3z"/><path fill-rule="evenodd" class="oprf-s11" d="m46 77l8 1 4 4-1 6-3 5-6 3-4-3-3-6 2-6z"/><path fill-rule="evenodd" class="oprf-s7" d="m50 79l6 2 1 6-5 2-4-4z"/><path fill-rule="evenodd" class="oprf-s10" d="m48 85l4 4 2 4-6 3-3-4v-5z"/><path fill-rule="evenodd" class="oprf-s11" d="m68 87l8-6 2 7-4 5h-10l-2-2z"/><path fill-rule="evenodd" class="oprf-s7" d="m72 84l4-3 2 7-4 2z"/><path fill-rule="evenodd" class="oprf-s0" d="m36 84l6-8 10-2-2 8-10 6-14 4-6-2z"/><path fill-rule="evenodd" class="oprf-s9" d="m28 78l14-2-6 8-16 6-17 6-2-2 11-5z"/><path fill-rule="evenodd" class="oprf-s5" d="m17 78h11l-16 11-11 5z"/><path fill-rule="evenodd" class="oprf-s1" d="m40 37l8-1 5 3 3 6v9l-1 9-3 8-5 6-30 2-3 1 5-6 5-9 5-10 5-9z"/><path fill-rule="evenodd" class="oprf-s7" d="m40 37l3 2-6 10-6 10-6 10-6 9-5 2 5-7 5-9 5-10 5-9z"/><path fill-rule="evenodd" class="oprf-s8" d="m43 39l5-3 5 3 3 6-8 3-9-2z"/><path fill-rule="evenodd" class="oprf-s5" d="m39 46l9 2 8-3v9h-3l-8-4-8-1z"/><path fill-rule="evenodd" class="oprf-s6" d="m37 49l8 1 1 8-1 6-14-5z"/><path fill-rule="evenodd" class="oprf-s12" d="m45 50l8 4 2 8-10 2 1-6z"/><path fill-rule="evenodd" class="oprf-s6" d="m31 59l7 2.5-8 16.2-11 0.3 6-9z"/><path fill-rule="evenodd" class="oprf-s12" d="m38 61.5l7 2.5-5 13-10 0.7z"/><path fill-rule="evenodd" class="oprf-s9" d="m45 64l10-2-3 9-5 6h-7z"/><path fill-rule="evenodd" class="oprf-s0" d="m48 26l6 4 3 6 5 3 1 4-5-1-4-3-6-3-6 1 3-5z"/><path fill-rule="evenodd" class="oprf-s10" d="m43 29l5-3-1 6z"/><path fill-rule="evenodd" class="oprf-s2" d="m52 3l4 3-3 3z"/><path fill-rule="evenodd" class="oprf-s5" d="m61 4l10-3h11l7 2 4 3 2 4-11 1-6 7-2 5v12l-4 3-10 1-5-3-3-6-6-4 1-6 2-6 2-5 3-3z"/><path fill-rule="evenodd" class="oprf-s8" d="m64 3.5l7-2.5h11l7 2 4 3 2 4-9-1-8-2-8-1z"/><path fill-rule="evenodd" class="oprf-s9" d="m51 14l4 5-1 11-6-4 1-6z"/><path fill-rule="evenodd" class="oprf-s9" d="m78 7l8 2 9 1-11 1-5 6z"/><path fill-rule="evenodd" class="oprf-s10" d="m55 19h6l1 4-1 6 3 4 6 2 2 3-10 1-5-3-3-6v-7z"/><path fill-rule="evenodd" class="oprf-s11" d="m54 23l4 3 1 6 4 4-1 3-5-3-3-6z"/><path fill-rule="evenodd" class="oprf-s9" d="m62 22l13 1.5 1 11.5h-6l-6-2-3-4z"/><path fill-rule="evenodd" class="oprf-s7" d="m66 26h1.2v3.8h4.8v1.2h-6z"/><path fill-rule="evenodd" class="oprf-s13" d="m76 22l4 4 6 6 3 5h-2l-11-2z"/><path fill-rule="evenodd" class="oprf-s3" d="m84 11l2 11 2 8 1 7-3-5-6-6-4-5 3-5z"/><path fill-rule="evenodd" class="oprf-s4" d="m84 11h10l2 7v8l-2 6-3 4-2 1-1-7-2-8z"/><path fill-rule="evenodd" class="oprf-s1" d="m83.5 14h2.5v2.5h-2.5z"/><path fill-rule="evenodd" class="oprf-s2" d="m94 11l4 2v6l-2 1v-2z"/><path fill-rule="evenodd" class="oprf-s1" d="m78.63 18.87l-4.76 4.76h-6.74l-4.76-4.76v-6.74l4.76-4.76h6.74l4.76 4.76z"/><path fill-rule="evenodd" class="oprf-s4" d="m77.15 18.26l-3.89 3.89h-5.52l-3.89-3.89v-5.52l3.89-3.89h5.52l3.89 3.89z"/><path fill-rule="evenodd" class="oprf-s2" d="m75.3 17.49l-2.81 2.81h-3.98l-2.81-2.81v-3.98l2.81-2.81h3.98l2.81 2.81z"/><path fill-rule="evenodd" class="oprf-s4" d="m68 13h3v3h-3z"/></svg>`;

  // ---- helpers ------------------------------------------------------------

  function el(tag, props, ...kids) {
    const n = document.createElement(tag);
    for (const [k, v] of Object.entries(props || {})) {
      if (k === "html") n.innerHTML = v;
      else n[k] = v;
    }
    for (const k of kids) if (k != null) n.append(k);
    return n;
  }

  /** FAT/Linux-safe path component (mirrors the syncer's rules loosely). */
  function sanitize(s, max = MAX_NAME) {
    let out = String(s == null ? "" : s)
      .replace(/[\u0000-\u001f]/g, "")
      .replace(/[\\/:*?"<>|]/g, "_")
      .replace(/\s+/g, " ")
      .trim()
      .replace(/[. ]+$/, "");
    if (out.length > max) out = out.slice(0, max).trim();
    return out || "_";
  }

  function extOf(url, fallback = "mp3") {
    try {
      const p = new URL(url, location.href).pathname;
      const m = /\.([a-z0-9]{2,5})$/i.exec(p);
      if (m) return m[1].toLowerCase();
    } catch (_) {
      /* ignore */
    }
    return fallback;
  }

  function pad2(n) {
    return String(n).padStart(2, "0");
  }

  function dayString(d) {
    return `${d.getFullYear()}-${pad2(d.getMonth() + 1)}-${pad2(d.getDate())}`;
  }

  function parseDate(s) {
    const d = s ? new Date(s) : null;
    return d && !isNaN(d.getTime()) ? d : null;
  }

  function childText(parent, name) {
    if (!parent) return "";
    for (const c of parent.children) {
      if (c.tagName === name || c.localName === name) return (c.textContent || "").trim();
    }
    const n = parent.getElementsByTagName(name)[0];
    return n ? (n.textContent || "").trim() : "";
  }

  function fmtDuration(raw) {
    if (!raw) return "";
    const s = String(raw).trim();
    if (/^\d+$/.test(s)) {
      const n = parseInt(s, 10);
      const h = Math.floor(n / 3600);
      const m = Math.floor((n % 3600) / 60);
      return h ? `${h}h${pad2(m)}` : `${m} min`;
    }
    return s;
  }

  function isFirefox() {
    return typeof window.showDirectoryPicker !== "function";
  }

  // ---- IndexedDB (remember the chosen folder across visits) ---------------

  function openDb() {
    return new Promise((resolve, reject) => {
      const r = indexedDB.open("opentone-rf", 1);
      r.onupgradeneeded = () => r.result.createObjectStore("kv");
      r.onsuccess = () => resolve(r.result);
      r.onerror = () => reject(r.error);
    });
  }

  async function idbGet(key) {
    const db = await openDb();
    return new Promise((resolve, reject) => {
      const q = db.transaction("kv").objectStore("kv").get(key);
      q.onsuccess = () => resolve(q.result);
      q.onerror = () => reject(q.error);
    });
  }

  async function idbSet(key, value) {
    const db = await openDb();
    return new Promise((resolve, reject) => {
      const q = db.transaction("kv", "readwrite").objectStore("kv").put(value, key);
      q.onsuccess = () => resolve();
      q.onerror = () => reject(q.error);
    });
  }

  async function pickDir() {
    const h = await window.showDirectoryPicker({ mode: "readwrite", id: "opentone-podcasts" });
    await idbSet(DIR_KEY, h).catch(() => {});
    return h;
  }

  async function rememberedDir() {
    const h = await idbGet(DIR_KEY).catch(() => null);
    if (!h) return null;
    try {
      if ((await h.queryPermission({ mode: "readwrite" })) === "granted") return h;
    } catch (_) {
      /* ignore */
    }
    return null;
  }

  async function ensureDir() {
    let dir = state.dir || (await rememberedDir());
    if (dir) {
      try {
        if ((await dir.queryPermission({ mode: "readwrite" })) !== "granted") {
          const p = await dir.requestPermission({ mode: "readwrite" });
          if (p !== "granted") dir = null;
        }
      } catch (_) {
        dir = null;
      }
    }
    if (!dir) dir = await pickDir();
    state.dir = dir;
    return dir;
  }

  async function subdir(root, station, show, create = true) {
    const a = await root.getDirectoryHandle(sanitize(station, 60), { create });
    return a.getDirectoryHandle(sanitize(show, 90), { create });
  }

  /** On-disk files under Station/Show as name(lower) → size in bytes (-1 if unreadable). */
  async function existingFiles(dir, station, show) {
    const map = new Map();
    try {
      const d = await subdir(dir, station, show, false);
      for await (const [name, h] of d.entries()) {
        if (h.kind !== "file") continue;
        try {
          const f = await h.getFile();
          map.set(name.toLowerCase(), f.size);
        } catch (_) {
          map.set(name.toLowerCase(), -1);
        }
      }
    } catch (_) {
      /* folder doesn't exist yet */
    }
    return map;
  }

  // ---- feed ---------------------------------------------------------------

  function feedUrl() {
    const links = document.querySelectorAll('link[rel="alternate"]');
    for (const l of links) {
      const type = (l.getAttribute("type") || "").toLowerCase();
      const href = l.getAttribute("href") || "";
      if (type.includes("rss") || /radiofrance-podcast\.net\/.*\.xml/i.test(href)) return l.href;
    }
    return null;
  }

  function stationFromPage() {
    const meta = document.querySelector('meta[property="og:site_name"]');
    if (meta && meta.content) return sanitize(meta.content, 60);
    const canon = document.querySelector('link[rel="canonical"]');
    if (canon) {
      try {
        const seg = (new URL(canon.href).pathname.split("/").filter(Boolean)[0] || "").toLowerCase();
        if (STATIONS[seg]) return STATIONS[seg];
        if (seg) return sanitize(seg, 60);
      } catch (_) {
        /* ignore */
      }
    }
    return FALLBACK_STATION;
  }

  function parseFeed(xmlText) {
    const doc = new DOMParser().parseFromString(xmlText, "application/xml");
    if (doc.getElementsByTagName("parsererror").length) throw new Error("Malformed feed");
    const channel = doc.getElementsByTagName("channel")[0];
    const show = sanitize(childText(channel, "title") || "Podcast", 90);
    const items = [];
    for (const it of doc.getElementsByTagName("item")) {
      const enc = it.getElementsByTagName("enclosure")[0];
      const url = enc ? enc.getAttribute("url") : null;
      const title = childText(it, "title");
      if (!url || !title) continue;
      items.push({
        title,
        url,
        date: parseDate(childText(it, "pubDate")),
        duration: fmtDuration(childText(it, "itunes:duration")),
      });
    }
    return { show, items };
  }

  function fetchText(url) {
    return fetch(url, { credentials: "omit", mode: "cors" }).then((r) => {
      if (!r.ok) throw new Error("HTTP " + r.status);
      return r.text();
    });
  }

  function gmFetchText(url) {
    return new Promise((resolve, reject) => {
      GM_xmlhttpRequest({
        method: "GET",
        url,
        onload: (r) => (r.status >= 200 && r.status < 300 ? resolve(r.responseText) : reject(new Error("HTTP " + r.status))),
        onerror: () => reject(new Error("network error")),
      });
    });
  }

  // ---- download -----------------------------------------------------------

  /** Fetches the audio with GM_xmlhttpRequest: the enclosure host answers with a
   * 302 (`proxycast…`) that carries no `Access-Control-Allow-Origin`, so a page
   * `fetch()` is blocked by CORS even though the redirect target is open. GM
   * bypasses CORS and follows the redirect. `onprogress` still drives the bar. */
  function gmBlob(url, onBytes) {
    return new Promise((resolve, reject) => {
      let total = 0;
      GM_xmlhttpRequest({
        method: "GET",
        url,
        responseType: "blob",
        timeout: 180000,
        onprogress: (e) => {
          total = e.total || total;
          onBytes(e.loaded || 0, e.total || 0);
        },
        onload: (r) => (r.status >= 200 && r.status < 300 ? resolve({ blob: r.response, total }) : reject(new Error("HTTP " + r.status))),
        onerror: () => reject(new Error("network error")),
        ontimeout: () => reject(new Error("timed out")),
      });
    });
  }

  /** The episode's byte size on Radio France, from HEAD (falling back to a
   * one-byte Range request). 0 = unknown. Used to check an on-disk file. */
  function remoteSize(url) {
    return new Promise((resolve) => {
      GM_xmlhttpRequest({
        method: "HEAD",
        url,
        timeout: 30000,
        onload: (r) => {
          const m = /content-length:\s*(\d+)/i.exec(r.responseHeaders || "");
          if (r.status >= 200 && r.status < 300 && m && Number(m[1]) > 0) return resolve(Number(m[1]));
          gmRangeSize(url).then(resolve);
        },
        onerror: () => gmRangeSize(url).then(resolve),
        ontimeout: () => resolve(0),
      });
    });
  }

  function gmRangeSize(url) {
    return new Promise((resolve) => {
      GM_xmlhttpRequest({
        method: "GET",
        url,
        headers: { Range: "bytes=0-0" },
        timeout: 30000,
        onload: (r) => {
          const m = /content-range:\s*bytes\s+\d+-\d+\/(\d+)/i.exec(r.responseHeaders || "");
          resolve(m ? Number(m[1]) : 0);
        },
        onerror: () => resolve(0),
        ontimeout: () => resolve(0),
      });
    });
  }

  async function saveEpisode(dir, station, show, filename, url, onBytes) {
    // Download fully first: getFileHandle({create:true}) would otherwise leave a
    // zero-length file behind when the transfer fails, which then reads as "on disk".
    const { blob, total } = await gmBlob(url, onBytes);
    if (!blob || blob.size === 0) throw new Error("empty download");
    if (total && blob.size !== total) throw new Error(`incomplete (${blob.size}/${total} bytes)`);

    const showDir = await subdir(dir, station, show, true);
    let existed = true;
    try {
      await showDir.getFileHandle(filename);
    } catch (_) {
      existed = false;
    }
    const w = await (await showDir.getFileHandle(filename, { create: true })).createWritable();
    try {
      await w.write(blob);
      await w.close();
      if (onBytes) onBytes(blob.size, blob.size);
    } catch (e) {
      if (w.abort) await w.abort().catch(() => {});
      if (!existed) await showDir.removeEntry(filename).catch(() => {});
      throw e;
    }
  }

  function gmDownload(station, show, filename, url) {
    return new Promise((resolve, reject) => {
      GM_download({
        url,
        name: filename,
        saveAs: false,
        onload: () => resolve(),
        onerror: (e) => reject(new Error(e && e.error ? e.error : "download failed")),
      });
    });
  }

  // ---- state & UI ---------------------------------------------------------

  const state = {
    feed: null,
    station: FALLBACK_STATION,
    show: "Podcast",
    episodes: [],
    dir: null,
    manual: new Set(),
    busy: false,
    stop: false,
    building: false,
  };

  function refreshExisting() {
    if (!state.dir) {
      for (const e of state.episodes) if (!state.manual.has(e.url)) e.existing = false;
      return Promise.resolve();
    }
    return existingFiles(state.dir, state.station, state.show).then((map) => {
      for (const e of state.episodes) {
        e.filename = episodeFilename(e);
        const size = map.get(e.filename.toLowerCase());
        e.size = size == null ? null : size;
        e.broken = size === 0; // an interrupted write can leave a zero-length file
        e.existing = size != null && size > 0;
        e.integrity = null;
      }
      return map;
    });
  }

  /** Default selection: new-only (uncheck what's already on disk). Only applied
   * when the folder is first known or on "New" — never during a download, so
   * the user's ticks survive. Rows ticked/unticked by hand are left alone. */
  function defaultSelection() {
    for (const e of state.episodes) if (!state.manual.has(e.url)) e.checked = !e.existing;
  }

  function episodeFilename(ep) {
    const day = ep.date ? dayString(ep.date) : "0000-00-00";
    return `${day} - ${sanitize(ep.title, MAX_NAME)}.${extOf(ep.url)}`;
  }

  let ui = null;

  function buildUi() {
    const panel = el("div", { id: PANEL_ID, className: "oprf oprf-collapsed" });
    const header = el("div", { className: "oprf-head" });
    const logo = el("button", { className: "oprf-logo", title: "opentone — click to show the episodes", html: LOGO_SVG });
    const title = el("div", { className: "oprf-title" });
    const collapse = el("button", { className: "oprf-btn oprf-icon oprf-collapse", title: "Collapse", textContent: "–" });
    header.append(logo, title, collapse);

    const controls = el("div", { className: "oprf-controls" });
    const folderBtn = el("button", { className: "oprf-btn", textContent: "Choose folder…", title: "Pick the folder to download into (e.g. ~/Podcasts)" });
    const allBtn = el("button", { className: "oprf-btn", textContent: "All" });
    const noneBtn = el("button", { className: "oprf-btn", textContent: "None" });
    const newBtn = el("button", { className: "oprf-btn", textContent: "New" });
    const refreshBtn = el("button", { className: "oprf-btn oprf-icon", title: "Re-check the folder", textContent: "⟳" });
    const verifyBtn = el("button", { className: "oprf-btn", textContent: "Verify", title: "Check the on-disk files against their size on Radio France" });
    controls.append(folderBtn, allBtn, noneBtn, newBtn, refreshBtn, verifyBtn);

    const list = el("div", { className: "oprf-list" });
    const footer = el("div", { className: "oprf-foot" });
    const status = el("div", { className: "oprf-status" });
    const downloadBtn = el("button", { className: "oprf-btn oprf-primary", textContent: "Download" });
    footer.append(status, downloadBtn);

    const body = el("div", { className: "oprf-body" }, controls, list, footer);
    panel.append(header, body);
    document.body.append(panel);

    logo.addEventListener("click", () => panel.classList.toggle("oprf-collapsed"));
    collapse.addEventListener("click", () => panel.classList.add("oprf-collapsed"));
    folderBtn.addEventListener("click", chooseFolder);
    allBtn.addEventListener("click", () => setAll("all"));
    noneBtn.addEventListener("click", () => setAll("none"));
    newBtn.addEventListener("click", () => setAll("new"));
    refreshBtn.addEventListener("click", () => refreshExisting().then(renderList));
    verifyBtn.addEventListener("click", verify);
    downloadBtn.addEventListener("click", run);

    ui = { panel, title, list, status, downloadBtn, folderBtn };
    return ui;
  }

  function setAll(mode) {
    for (const e of state.episodes) {
      state.manual.delete(e.url);
      e.checked = mode === "all" ? true : mode === "none" ? false : !e.existing;
    }
    renderList();
  }

  function badge(text, kind) {
    return el("span", { className: "oprf-badge oprf-" + kind, textContent: text });
  }

  function spinner() {
    return el("span", { className: "oprf-spinner" });
  }

  /** The live per-row indicator: spinner + %, a tick, or a cross. */
  function renderRowStatus(ep) {
    const node = ep.el;
    if (!node) return;
    node.className = "oprf-rowstatus";
    node.textContent = "";
    if (ep.status === "downloading") {
      node.append(spinner());
      node.append(document.createTextNode(ep.percent ? ` ${ep.percent}%` : ""));
    } else if (ep.status === "done") {
      node.classList.add("oprf-done");
      node.textContent = "✓";
    } else if (ep.status === "error") {
      node.classList.add("oprf-error");
      node.textContent = "✗";
    }
  }

  function renderList() {
    if (!ui) return;
    const list = ui.list;
    list.textContent = "";
    if (!state.episodes.length) {
      list.append(el("div", { className: "oprf-empty", textContent: "No episodes in this feed." }));
    }
    for (const ep of state.episodes) {
      const row = el("label", { className: "oprf-row" + (ep.checked ? " oprf-sel" : "") });
      const cb = el("input", { type: "checkbox" });
      cb.checked = !!ep.checked;
      cb.addEventListener("change", () => {
        ep.checked = cb.checked;
        state.manual.add(ep.url);
        row.classList.toggle("oprf-sel", cb.checked);
        updateFooter();
      });
      const meta = el("div", { className: "oprf-meta" });
      const name = el("div", { className: "oprf-name", textContent: ep.title, title: ep.title });
      const sub = el("div", { className: "oprf-sub" });
      const bits = [];
      if (ep.date) bits.push(dayString(ep.date));
      if (ep.duration) bits.push(ep.duration);
      sub.append(document.createTextNode(bits.join("  ·  ")));
      if (ep.broken) sub.append(badge("empty — re-download", "warn"));
      else if (ep.integrity === "bad") sub.append(badge("size mismatch", "bad"));
      else if (ep.integrity === "ok") sub.append(badge("on disk ✓", "ok"));
      else if (ep.existing) sub.append(badge("on disk", "ok"));
      meta.append(name, sub);
      const status = el("div", { className: "oprf-rowstatus" });
      ep.el = status;
      row.append(cb, meta, status);
      renderRowStatus(ep);
      list.append(row);
    }
    updateFooter();
  }

  function updateFooter() {
    if (!ui) return;
    const chosen = state.episodes.filter((e) => e.checked).length;
    ui.status.textContent = `${chosen} / ${state.episodes.length} selected`;
    ui.downloadBtn.disabled = state.busy || chosen === 0;
  }

  function render() {
    if (!ui) return;
    const dirName = state.dir ? state.dir.name : "not chosen";
    ui.title.textContent = state.show;
    ui.title.title = `${state.station} — ${state.show}`;
    ui.folderBtn.textContent = state.dir ? `Folder: ${state.dir.name}` : "Choose folder…";
    ui.folderBtn.title = isFirefox()
      ? "Firefox can't write to a chosen folder; downloads go to your Downloads folder."
      : "Pick the folder to download into (e.g. ~/Podcasts)";
    ui.status.dataset.dir = dirName;
    renderList();
  }

  async function chooseFolder() {
    if (isFirefox()) {
      alert("This browser can't write to a folder you pick (needs Chrome or Edge). Downloads will go to your default Downloads folder.");
      return;
    }
    try {
      state.dir = await pickDir();
      await refreshExisting();
      defaultSelection();
      render();
    } catch (_) {
      /* cancelled */
    }
  }

  /** Checks each on-disk episode's size against the size Radio France reports. */
  async function verify() {
    if (state.busy) return;
    const targets = state.episodes.filter((e) => e.existing);
    if (!state.dir) {
      ui.status.textContent = "Choose a folder first";
      return;
    }
    if (!targets.length) {
      ui.status.textContent = "Nothing on disk to verify";
      return;
    }
    state.busy = true;
    updateFooter();
    let bad = 0;
    try {
      for (let i = 0; i < targets.length; i++) {
        const ep = targets[i];
        ui.status.textContent = `Verify ${i + 1}/${targets.length} — ${ep.title.slice(0, 40)}`;
        const expected = await remoteSize(ep.url);
        ep.integrity = !expected ? "unknown" : expected === ep.size ? "ok" : "bad";
        if (ep.integrity === "bad") bad++;
        renderList();
      }
    } finally {
      state.busy = false;
      render();
      ui.status.textContent = bad ? `${bad} file(s) don't match — re-download them` : "All on-disk files verified";
    }
  }

  async function run() {
    if (state.busy) return;
    state.busy = true;
    state.stop = false;
    updateFooter();

    let ok = 0;
    let fail = 0;

    try {
      for (const ep of state.episodes) {
        ep.status = undefined;
        ep.percent = 0;
      }
      if (!isFirefox()) {
        try {
          await ensureDir();
        } catch (_) {
          return; // cancelled the picker
        }
        await refreshExisting();
        defaultSelection(); // new-only, now that the folder is known
        render();
      } else {
        renderList();
      }

      const selected = state.episodes.filter((e) => e.checked);
      for (let i = 0; i < selected.length; i++) {
        if (state.stop) break;
        const ep = selected[i];
        const filename = episodeFilename(ep);
        const label = `${i + 1}/${selected.length}`;
        ep.status = "downloading";
        ep.percent = 0;
        renderRowStatus(ep);
        ui.status.textContent = `${label} — ${ep.title.slice(0, 60)}`;
        try {
          if (isFirefox()) {
            await gmDownload(state.station, state.show, filename, ep.url);
          } else {
            await saveEpisode(state.dir, state.station, state.show, filename, ep.url, (done, total) => {
              ep.percent = total ? Math.round((done / total) * 100) : 0;
              renderRowStatus(ep);
              const pct = total ? ` ${ep.percent}%` : ` ${(done / 1048576).toFixed(1)} MB`;
              ui.status.textContent = `${label} — ${ep.title.slice(0, 48)}${pct}`;
            });
          }
          ep.existing = true;
          ep.checked = false; // downloaded: drop it from the selection
          state.manual.delete(ep.url);
          ep.status = "done";
          renderRowStatus(ep);
          ok++;
        } catch (e) {
          ep.status = "error";
          renderRowStatus(ep);
          fail++;
          console.warn("[opentone] download failed:", ep.title, e);
        }
      }
    } finally {
      state.busy = false;
      await refreshExisting().catch(() => {});
      render();
      ui.status.textContent = state.stop
        ? `Stopped — ${ok} downloaded, ${fail} failed`
        : `Done — ${ok} downloaded${fail ? `, ${fail} failed` : ""}`;
    }
  }

  // ---- boot / SPA navigation ---------------------------------------------

  async function init(url) {
    if (state.building) return;
    state.building = true;
    try {
      let text;
      try {
        text = await fetchText(url);
      } catch (_) {
        text = await gmFetchText(url);
      }
      const parsed = parseFeed(text);
      if (!parsed.items.length) return;

      state.feed = url;
      state.station = stationFromPage();
      state.show = parsed.show;
      state.manual = new Set();
      state.episodes = parsed.items.map((it) => ({ ...it, filename: episodeFilename(it), existing: false, checked: true }));

      if (!ui) buildUi();
      ui.panel.dataset.feed = url;
      render();

      // If a folder was already granted, mark what is on disk.
      const dir = await rememberedDir();
      if (dir) {
        state.dir = dir;
        await refreshExisting();
        defaultSelection();
        render();
      }
    } catch (e) {
      console.warn("[opentone] feed load failed:", e);
    } finally {
      state.building = false;
    }
  }

  function boot() {
    const url = feedUrl();
    if (!url) return;
    const panel = document.getElementById(PANEL_ID);
    if (panel && panel.dataset.feed === url && !state.busy) return;
    init(url);
  }

  injectStyle();

  const observer = new MutationObserver(() => boot());
  observer.observe(document.head, { childList: true, subtree: true });

  // Initial + a few retries while the SPA hydrates its <head>.
  boot();
  let tries = 0;
  const timer = setInterval(() => {
    boot();
    if (++tries > 20) clearInterval(timer);
  }, 500);

  function injectStyle() {
    if (document.getElementById(STYLE_ID)) return;
    const css = `
      #${PANEL_ID} { position: fixed; right: 16px; bottom: 16px; width: 380px; max-width: calc(100vw - 32px);
        max-height: min(70vh, 640px); display: flex; flex-direction: column; z-index: 2147483000;
        background: #14161a; color: #e9eaec; border: 1px solid #2a2e35; border-radius: 12px;
        box-shadow: 0 12px 40px rgba(0,0,0,.45); font: 13px/1.4 system-ui, sans-serif; overflow: hidden; }
      #${PANEL_ID}.oprf-collapsed .oprf-body { display: none; }
      #${PANEL_ID} .oprf-head { display: flex; align-items: center; gap: 8px; padding: 10px 12px;
        background: #1b1e24; border-bottom: 1px solid #2a2e35; }
      #${PANEL_ID} .oprf-title { flex: 1; font-weight: 600; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
      #${PANEL_ID} .oprf-logo { width: 34px; height: 34px; padding: 0; border: 0; background: transparent; cursor: pointer; flex: none; }
      #${PANEL_ID} .oprf-logo svg { width: 100%; height: 100%; display: block; }
      #${PANEL_ID}.oprf-collapsed { width: auto; max-height: none; overflow: visible; background: transparent; border: 0; box-shadow: none; }
      #${PANEL_ID}.oprf-collapsed .oprf-head { background: #14161a; border: 1px solid #2a2e35; border-radius: 50%; padding: 8px; justify-content: center; box-shadow: 0 8px 24px rgba(0,0,0,.45); }
      #${PANEL_ID}.oprf-collapsed .oprf-title,
      #${PANEL_ID}.oprf-collapsed .oprf-collapse,
      #${PANEL_ID}.oprf-collapsed .oprf-body { display: none; }
      #${PANEL_ID} .oprf-body { display: flex; flex-direction: column; min-height: 0; }
      #${PANEL_ID} .oprf-controls { display: flex; flex-wrap: wrap; gap: 6px; padding: 8px 12px; }
      #${PANEL_ID} .oprf-list { overflow-y: auto; padding: 0 6px 0 12px; }
      #${PANEL_ID} .oprf-row { display: flex; gap: 10px; align-items: flex-start; padding: 7px 6px;
        border-radius: 8px; cursor: pointer; }
      #${PANEL_ID} .oprf-row:hover { background: #1c1f26; }
      #${PANEL_ID} .oprf-row.oprf-sel { background: #1d2b4d; }
      #${PANEL_ID} .oprf-row.oprf-sel:hover { background: #22335c; }
      #${PANEL_ID} .oprf-row input { margin-top: 3px; flex: none; }
      #${PANEL_ID} .oprf-meta { flex: 1; min-width: 0; }
      #${PANEL_ID} .oprf-name { white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
      #${PANEL_ID} .oprf-sub { color: #9aa2ad; font-size: 11px; display: flex; gap: 8px; align-items: center; }
      #${PANEL_ID} .oprf-badge { border-radius: 6px; padding: 0 5px; font-size: 10px; }
      #${PANEL_ID} .oprf-badge.oprf-ok { background: #24503a; color: #b8f0cd; }
      #${PANEL_ID} .oprf-badge.oprf-bad { background: #5a2326; color: #ffc4c4; }
      #${PANEL_ID} .oprf-badge.oprf-warn { background: #5a4a1f; color: #ffe6a8; }
      #${PANEL_ID} .oprf-rowstatus { flex: none; min-width: 42px; display: flex; align-items: center; justify-content: flex-end; gap: 6px; font-size: 11px; color: #9aa2ad; }
      #${PANEL_ID} .oprf-rowstatus.oprf-done { color: #7fe0a3; }
      #${PANEL_ID} .oprf-rowstatus.oprf-error { color: #ff9b9b; }
      #${PANEL_ID} .oprf-spinner { width: 12px; height: 12px; border: 2px solid #333a44; border-top-color: #7aa2ff; border-radius: 50%; animation: oprf-spin 0.8s linear infinite; }
      @keyframes oprf-spin { to { transform: rotate(360deg); } }
      #${PANEL_ID} .oprf-empty { color: #9aa2ad; padding: 12px; }
      #${PANEL_ID} .oprf-foot { display: flex; align-items: center; gap: 8px; padding: 10px 12px;
        border-top: 1px solid #2a2e35; background: #1b1e24; }
      #${PANEL_ID} .oprf-status { flex: 1; font-size: 11px; color: #9aa2ad; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
      #${PANEL_ID} .oprf-btn { background: #262b33; color: #e9eaec; border: 1px solid #333a44; border-radius: 8px;
        padding: 4px 9px; cursor: pointer; font: inherit; }
      #${PANEL_ID} .oprf-btn:hover { background: #2f3540; }
      #${PANEL_ID} .oprf-btn:disabled { opacity: .45; cursor: default; }
      #${PANEL_ID} .oprf-icon { padding: 4px 8px; }
      #${PANEL_ID} .oprf-primary { background: #2f6df6; border-color: #2f6df6; }
      #${PANEL_ID} .oprf-primary:hover:not(:disabled) { background: #3d79f8; }
    `;
    document.head.append(el("style", { id: STYLE_ID, textContent: css }));
  }
})();
