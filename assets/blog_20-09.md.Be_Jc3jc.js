import{_ as a,a as n,o as i,Y as p}from"./chunks/framework.BFAekxW2.js";const o=JSON.parse('{"title":"Covers","description":"","frontmatter":{},"headers":[],"relativePath":"blog/20-09.md","filePath":"blog/20-09.md"}'),e={name:"blog/20-09.md"};function l(t,s,h,k,r,d){return i(),n("div",null,s[0]||(s[0]=[p(`<h1 id="covers" tabindex="-1">Covers <a class="header-anchor" href="#covers" aria-label="Permalink to “Covers”">​</a></h1><p>Let&#39;s show the albums&#39;s covers!!!</p><p>There are a few steps/questions to this:</p><p><strong>Steps</strong></p><ul><li>Generate nice 128x128 covers</li><li>Index them properlly</li><li>Display them!</li></ul><p><strong>Questions</strong></p><ul><li>How to get small but still nice images?</li><li>What image format to pick ? (POI or raw RGB?)</li><li>Should I buffer them on the PSRAM ?</li></ul><h2 id="generate-the-covers" tabindex="-1">Generate the covers <a class="header-anchor" href="#generate-the-covers" aria-label="Permalink to “Generate the covers”">​</a></h2><p>Turns out the answer on <em>how to get nice small is images</em> is simple: <code>Lanczos Resampling</code> ! It is standard, I just never heard of it before, but it creates much better results on drastic scaledown than naive gaussian approaches.</p><h2 id="index-the-covers" tabindex="-1">Index the covers <a class="header-anchor" href="#index-the-covers" aria-label="Permalink to “Index the covers”">​</a></h2><p>Covers are by album, I did not have track_to_album indexing, so I first need it for arbitrary access ( I could get tracks from a playlist or from the general tracks/ index etc).</p><p>Also, some albums might not have a cover, so we need a fallback (basically the cover index 0, with some image)</p><p>Then I need to generate a big texture with all the images.</p><p>And finally I need a album_to_cover index and.... tadaaa</p><h2 id="display-them" tabindex="-1">Display them <a class="header-anchor" href="#display-them" aria-label="Permalink to “Display them”">​</a></h2><p>Now this is the trickiest part: I did not know if I should compress the images to minimize storage on the SD, bandwidth during transferts or if I should just store them as plain RGB.</p><p>So I did a benchmark:</p><p>main_cover_benchmark.cpp</p><div class="language-cpp"><button title="Copy Code" class="copy"></button><span class="lang">cpp</span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// Standalone on-device benchmark: flash the cover_benchmark PlatformIO env</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// (see platformio.ini) instead of the real firmware to compare QOI vs</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// raw565 album-art loading on actual hardware. Lives in src/ (not</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// test/onboard/, where the actual benchmark logic lives) purely because</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// PlatformIO&#39;s build_src_filter can only select files within src_dir, and</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// per-environment src_dir overrides aren&#39;t honored by this PlatformIO</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// version (6.2.0) - see the comment on env:cover_benchmark in</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// platformio.ini. build_src_filter excludes this file from the real</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// nucleo_f446re env, and excludes main.cpp from this one, so exactly one</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// setup()/loop() pair ever links.</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">//</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// See .agents/covers-plan.md, Phase 0, and test/onboard/CoverBenchmark.h</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// for what&#39;s measured.</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">//</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// Before flashing: copy both indexer/generate-benchmark-covers.ts output</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// folders (bench_qoi/, bench_raw565/) onto the SD card under /openpod/.</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#include</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> &lt;Arduino.h&gt;</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#include</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> &lt;SdFat.h&gt;</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#include</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> &quot;pinout.h&quot;</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#include</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> &quot;rendering/ILI9341_GFX.h&quot;</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#include</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> &quot;storage/PSRAM_controller.hpp&quot;</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">#include</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;"> &quot;../test/onboard/CoverBenchmark.h&quot;</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">SdFat sd;</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">ILI9341_GFX display;</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// CoverBenchmark holds ~73KB of buffers (COVER_BYTES_565 + MAX_QOI_BYTES,</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// see CoverBenchmark.h) - global/static storage (.bss), not a local inside</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// setup(). A local of this size would blow setup()&#39;s stack frame the</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// instant the function is entered (C++ reserves the full frame upfront,</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// not lazily at each variable&#39;s declaration point), corrupting memory</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// before any of its code even runs - this crashed psram.init() on real</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// hardware even though the corruption&#39;s root cause was this line, not that</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">// function.</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">onboard_bench</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">::</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">CoverBenchmark</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> bench</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">sd</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">, </span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">display</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> setup</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">() {</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  Serial.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">begin</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">115200</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">  while</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> (</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">!</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">Serial)</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">    delay</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">10</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  Serial.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">println</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;=== Cover format benchmark (QOI vs raw565) ===&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">  // SD must be brought up before PSRAM - matches Audio_buffer::begin()&#39;s</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">  // order (src/sound/Audio_buffer.cpp), which is the only other place in</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">  // this codebase that initializes both. Doing it the other way around</span></span>
<span class="line"><span style="--shiki-light:#6A737D;--shiki-dark:#6A737D;">  // hangs inside psram.init() on real hardware (confirmed on-device).</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">  if</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> (</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">!</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">sd.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">begin</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">SdSpiConfig</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(CARDCS, SHARED_SPI, </span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">SD_SCK_MHZ</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">25</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">)))) {</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">    Serial.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">println</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;ERROR: SD card init failed&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    while</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> (</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">1</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">) </span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">delay</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">1000</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  }</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  Serial.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">println</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;SD ready&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">  if</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> (</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">!</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">psram.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">init</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">()) {</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">    Serial.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">println</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;ERROR: PSRAM init failed&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">    while</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> (</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">1</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">) </span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">delay</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">1000</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  }</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  Serial.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">println</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;PSRAM ready&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  display.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">begin</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">();</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  display.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">fillScreen</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">0x</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">0000</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  Serial.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">println</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;Display ready&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">  delay</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">200</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">  auto</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> qoiStats </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> bench.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">runSweep</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;QOI&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">, </span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;/openpod/bench_qoi/thumbs.bin&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">,</span></span>
<span class="line"><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">                                 &quot;/openpod/bench_qoi/album_to_cover.bin&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  bench.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">printStats</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;QOI&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">, qoiStats);</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">  delay</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">500</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">  auto</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;"> raw565Stats </span><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">=</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">      bench.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">runSweep</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;raw565&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">, </span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;/openpod/bench_raw565/thumbs.bin&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">,</span></span>
<span class="line"><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">                     &quot;/openpod/bench_raw565/album_to_cover.bin&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  bench.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">printStats</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;raw565&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">, raw565Stats);</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">  delay</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">500</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  bench.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">benchmarkPsramCacheRoundtrip</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">();</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  Serial.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">println</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">  Serial.</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">println</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#032F62;--shiki-dark:#9ECBFF;">&quot;=== Benchmark complete ===&quot;</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">}</span></span>
<span class="line"></span>
<span class="line"><span style="--shiki-light:#D73A49;--shiki-dark:#F97583;">void</span><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;"> loop</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">() {</span></span>
<span class="line"><span style="--shiki-light:#6F42C1;--shiki-dark:#B392F0;">  delay</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">(</span><span style="--shiki-light:#005CC5;--shiki-dark:#79B8FF;">1000</span><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">);</span></span>
<span class="line"><span style="--shiki-light:#24292E;--shiki-dark:#E1E4E8;">}</span></span></code></pre></div><p>Platormio.ini</p><div class="language-"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>[env:cover_benchmark]</span></span>
<span class="line"><span>platform = ststm32</span></span>
<span class="line"><span>board = nucleo_f446re</span></span>
<span class="line"><span>framework = arduino</span></span>
<span class="line"><span>debug_tool = stlink</span></span>
<span class="line"><span>monitor_speed = 115200</span></span>
<span class="line"><span>upload_protocol = stlink</span></span>
<span class="line"><span>build_src_filter = -&lt;*&gt; +&lt;main_cover_benchmark.cpp&gt; +&lt;storage/PSRAM_controller.cpp&gt;</span></span>
<span class="line"><span>build_flags =</span></span>
<span class="line"><span>	-DUSE_UTF8_LONG_NAMES=1</span></span>
<span class="line"><span>lib_deps =</span></span>
<span class="line"><span>	adafruit/Adafruit GFX Library@^1.11.9</span></span>
<span class="line"><span>	adafruit/Adafruit BusIO@^1.15.0</span></span>
<span class="line"><span>	arduino-libraries/SD@^1.3.0</span></span>
<span class="line"><span>	greiman/SdFat@^2.3.0</span></span></code></pre></div><p>And some <a href="/open-pod/blog/bench-fixtures.zip">benchmark files</a></p><p>I got this:</p><div class="language-"><button title="Copy Code" class="copy"></button><span class="lang"></span><pre class="shiki shiki-themes github-light github-dark" style="--shiki-light:#24292e;--shiki-dark:#e1e4e8;--shiki-light-bg:#fff;--shiki-dark-bg:#24292e;" tabindex="0" dir="ltr"><code><span class="line"><span>=== Benchmark complete ===</span></span>
<span class="line"><span>=== Cover format benchmark (QOI vs raw565) ===</span></span>
<span class="line"><span>SD ready</span></span>
<span class="line"><span>=== PSRAM INIT START ===</span></span>
<span class="line"><span>PSRAM ID attempt 0: 0xD 0x5D</span></span>
<span class="line"><span>PSRAM OK</span></span>
<span class="line"><span>PSRAM ready</span></span>
<span class="line"><span>Display ready</span></span>
<span class="line"><span>  loadCoverIndex /openpod/bench_qoi/album_to_cover.bin: fileSize=229 headerBytesRead=4 entryCount=25</span></span>
<span class="line"><span>    entry 0: offset=0 length=36513 format=0</span></span>
<span class="line"><span>    entry 1: offset=36513 length=37199 format=0</span></span>
<span class="line"><span>    entry 2: offset=73712 length=28092 format=0</span></span>
<span class="line"><span>=== QOI sweep ===</span></span>
<span class="line"><span>  album 0: read=27902us decode=5115us</span></span>
<span class="line"><span>  album 1: read=28049us decode=5334us</span></span>
<span class="line"><span>  album 2: read=21892us decode=4730us</span></span>
<span class="line"><span>  album 3: read=2126us decode=781us</span></span>
<span class="line"><span>  album 4: read=19365us decode=4603us</span></span>
<span class="line"><span>  album 5: read=2722us decode=781us</span></span>
<span class="line"><span>  album 6: read=2722us decode=781us</span></span>
<span class="line"><span>  album 7: read=2721us decode=781us</span></span>
<span class="line"><span>  album 8: read=2722us decode=781us</span></span>
<span class="line"><span>  album 9: read=2723us decode=781us</span></span>
<span class="line"><span>  album 10: read=2722us decode=781us</span></span>
<span class="line"><span>  album 11: read=2722us decode=781us</span></span>
<span class="line"><span>  album 12: read=2722us decode=781us</span></span>
<span class="line"><span>  album 13: read=2725us decode=780us</span></span>
<span class="line"><span>  album 14: read=2723us decode=781us</span></span>
<span class="line"><span>  album 15: read=2724us decode=780us</span></span>
<span class="line"><span>  album 16: read=2724us decode=781us</span></span>
<span class="line"><span>  album 17: read=2722us decode=781us</span></span>
<span class="line"><span>  album 18: read=2722us decode=781us</span></span>
<span class="line"><span>  album 19: read=2724us decode=781us</span></span>
<span class="line"><span>  album 20: read=2722us decode=781us</span></span>
<span class="line"><span>  album 21: read=2723us decode=781us</span></span>
<span class="line"><span>  album 22: read=2722us decode=781us</span></span>
<span class="line"><span>  album 23: read=2722us decode=781us</span></span>
<span class="line"><span>  album 24: read=2724us decode=780us</span></span>
<span class="line"><span>--- QOI summary ---</span></span>
<span class="line"><span>  covers rendered: 25</span></span>
<span class="line"><span>  avg read: 4736us  max read: 26634us</span></span>
<span class="line"><span>  avg decode: 1447us  max decode: 5334us</span></span>
<span class="line"><span>  avg total (read+decode): 6184us</span></span>
<span class="line"><span>  loadCoverIndex /openpod/bench_raw565/album_to_cover.bin: fileSize=229 headerBytesRead=4 entryCount=25</span></span>
<span class="line"><span>    entry 0: offset=0 length=32768 format=1</span></span>
<span class="line"><span>    entry 1: offset=32768 length=32768 format=1</span></span>
<span class="line"><span>    entry 2: offset=65536 length=32768 format=1</span></span>
<span class="line"><span>=== raw565 sweep ===</span></span>
<span class="line"><span>  album 0: read=24764us decode=0us</span></span>
<span class="line"><span>  album 1: read=24882us decode=0us</span></span>
<span class="line"><span>  album 2: read=24888us decode=0us</span></span>
<span class="line"><span>  album 3: read=24877us decode=0us</span></span>
<span class="line"><span>  album 4: read=24878us decode=0us</span></span>
<span class="line"><span>  album 5: read=24880us decode=0us</span></span>
<span class="line"><span>  album 6: read=24878us decode=0us</span></span>
<span class="line"><span>  album 7: read=24880us decode=0us</span></span>
<span class="line"><span>  album 8: read=24879us decode=0us</span></span>
<span class="line"><span>  album 9: read=24879us decode=0us</span></span>
<span class="line"><span>  album 10: read=24880us decode=0us</span></span>
<span class="line"><span>  album 11: read=24878us decode=0us</span></span>
<span class="line"><span>  album 12: read=24881us decode=0us</span></span>
<span class="line"><span>  album 13: read=24878us decode=0us</span></span>
<span class="line"><span>  album 14: read=24881us decode=0us</span></span>
<span class="line"><span>  album 15: read=24878us decode=0us</span></span>
<span class="line"><span>  album 16: read=24879us decode=0us</span></span>
<span class="line"><span>  album 17: read=24880us decode=0us</span></span>
<span class="line"><span>  album 18: read=24879us decode=0us</span></span>
<span class="line"><span>  album 19: read=24881us decode=0us</span></span>
<span class="line"><span>  album 20: read=24879us decode=0us</span></span>
<span class="line"><span>  album 21: read=24879us decode=0us</span></span>
<span class="line"><span>  album 22: read=24880us decode=0us</span></span>
<span class="line"><span>  album 23: read=24879us decode=0us</span></span>
<span class="line"><span>  album 24: read=24881us decode=0us</span></span>
<span class="line"><span>--- raw565 summary ---</span></span>
<span class="line"><span>  covers rendered: 25</span></span>
<span class="line"><span>  avg read: 23460us  max read: 23473us</span></span>
<span class="line"><span>  avg decode: 0us  max decode: 0us</span></span>
<span class="line"><span>  avg total (read+decode): 23460us</span></span>
<span class="line"><span>=== PSRAM cover-cache roundtrip ===</span></span>
<span class="line"><span>  PSRAM write (32KB): 77957us</span></span>
<span class="line"><span>  PSRAM read (32KB): 78683us</span></span>
<span class="line"><span></span></span>
<span class="line"><span>=== Benchmark complete ===</span></span></code></pre></div><h3 id="conclusion" tabindex="-1">Conclusion <a class="header-anchor" href="#conclusion" aria-label="Permalink to “Conclusion”">​</a></h3><p><strong>raw565 it is.</strong> much faser, less code, not much larger space on the disk.</p>`,26)]))}const E=a(e,[["render",l]]);export{o as __pageData,E as default};
