# Handoff: Nucleo Pinout Studio

## Overview

An interactive pinout viewer/editor for the **STM32F446RE (LQFP64)** as wired on the
**Nucleo-F446RE** board, for an MP3 player project. It replaces an earlier VitePress
component whose editor was a raw JSON textarea, whose filters did not highlight what they
selected, and whose pin cells carried two competing labels.

The page lets the user:

- see the pin assignment on the **Nucleo headers**, on the **LQFP64 package**, or as a
  **searchable pin table**;
- filter/highlight by peripheral, by alternate-function family (SPI1, I2C3, TIM2 …), by pin
  category, or by free-text search;
- inspect a single pin (package number, board names, default function, all alternate
  functions, I/O structure, current assignments, board-level gotchas);
- **edit the mapping** in a spreadsheet-style table or via quick-assign on the inspector,
  with live design checks;
- export the mapping as a **C++ header**, a **Markdown table**, or **JSON**.

## About the design files

The files in this bundle are **design references written in HTML** — a working prototype of
the intended look and behaviour, not production code to lift. The task is to **recreate this
design in the target codebase's environment** (the original lives in a VitePress/Vue docs
site, so Vue SFCs are the natural target) using its established component patterns, styling
approach and build. Where no environment exists yet, pick the framework that fits the project
and implement the design there.

`Nucleo Pinout Studio.dc.html` is a single self-rendering file: a template plus a logic class.
`support.js` is only the prototyping runtime that renders it — **do not port `support.js`**.
Everything worth porting is in the template markup, the `Component` logic class, and the data
tables described below.

## Fidelity

**High fidelity.** Colours, type, spacing, states and interactions are final and should be
reproduced closely. The one deliberately loose part is the exact pixel geometry of the LQFP64
drawing — it is a CSS grid of pads, not a schematic-accurate footprint, and may be redrawn as
long as the reading order (pin 1 top-left, counter-clockwise) survives.

## Data model

Three static data tables plus one editable mapping. All live in the logic class today; in a
real app they belong in separate modules, and the mapping should be the file the docs site
already owns (`docs/.vitepress/theme/components/pinmap/mapping.json`).

### 1. `RAW` — the 64 package pins (static, from DS10693)

Array of tuples: `[number, name, type, defaultFunction, alternateFunctions, additionalFunctions, arduinoName, morphoName]`.

- `type`: `'FT'` (5 V tolerant), `'FTf'` (5 V tolerant, FM+ I2C), `'TTa'` (3.3 V only, analog),
  `'P'` (power), `'R'` (reset), `'B'` (boot).
- `alternateFunctions` / `additionalFunctions` are comma-space separated strings.
- Derived per pin at load:
  - `base` = `name.split('-')[0]` (`PA0-WKUP` → `PA0`, `PC14-OSC32_IN` → `PC14`).
  - `cat` (category): `power` / `reset` / `boot` from `type`; `debug` when the default function
    matches `/SWD|JTDI|JTDO|NJTRST|JTMS|JTCK/`; otherwise `io`.
  - `blob` = default + alternates + additional joined, used for all function matching.

### 2. `CONN` — the six board connectors (static, from UM1724)

One entry per connector: `{ id, title, cols, side, pins[] }`.

- `cols: 2` for the 38-pin morpho headers CN7 and CN10, `1` for CN5/CN6/CN8/CN9.
- `side: 'L' | 'R'` places the connector group left or right of the MCU.
- Each `pins[]` entry is a string: `"PA5"` (silkscreen name equals MCU pin),
  `"A2|PA4"` (silkscreen | MCU pin), `"GND"` (no MCU pin), `"—"` (not connected).
- Order is **header pin order**: for 2-column connectors, index 0 is pin 1 (left column),
  index 1 is pin 2 (right column), and so on interleaved. Grid position is therefore
  `column = (i % 2) + 1`, `row = floor(i / 2) + 1`.

### 3. `NOTES` — board-level gotchas (static)

`{ PA5: 'Also drives LD2…', PC13: 'Also wired to B1…', PC14/PC15: LSE, PH0/PH1: HSE }`.
Shown as a "Board" row in the inspector. These are **notes, not checks** — deliberately kept
out of the design-checks panel.

### 4. Mapping rows (editable, persisted)

Flat array, one row per assignment:

```ts
{ p: string;      // peripheral, e.g. "VS1053"
  bus: string;    // e.g. "SPI1", "16-bit parallel"
  pin: string;    // MCU base name, e.g. "PA5"
  role: string;   // e.g. "SPI1_SCK", "MP3CS (chip select)"
  macro: string } // optional C #define name, e.g. "MP3CS"
```

Flat is the source of truth; the grouped JSON shape (`[{peripheral, bus, pins:[{pin, role, macro}]}]`)
is produced only on export and parsed back on import, so the docs site's existing
`mapping.json` format is preserved. The firmware default is 37 rows over 5 peripherals
(VS1053/SPI1, SD card/SPI1, PSRAM/SPI2, Screen ILI9341/16-bit parallel, MPR121/I2C3), read from
`software/src/pinout.h`, `software/src/rendering/ILI9341_driver.h` and the
`TwoWire Wire3(PB4, PA8)` setup in `software/src/main.cpp`.

Persistence: `localStorage['nucleo-pinmap-v1']`, written on every mutation. "Reset to firmware"
restores the built-in defaults.

## Layout

Single scrolling column, `max-width: 1680px`, `padding: 20px 22px 40px`, vertical `gap: 14px`.

1. **Header** — flex row, wraps, `border-bottom: 1px solid oklch(0.30 0.014 255)`,
   `padding-bottom: 14px`. Left: eyebrow "PINOUT STUDIO" (11px mono, `letter-spacing: 0.14em`,
   uppercase) over an H1 (26px/600, `letter-spacing: -0.01em`) reading
   "STM32F446RE / Nucleo-F446RE", the second half in the muted grey. Then a 52ch intro
   paragraph (13px, `line-height: 1.5`). Right, pushed by `margin-left: auto`: three stats —
   Assigned, Free I/O (green), Flags (amber) — each a 10px uppercase mono label over a 20px/600
   mono number.
2. **Toolbar** — one card (`background: oklch(0.235 0.014 255)`, `border-radius: 10px`,
   `padding: 10px 12px`, `gap: 10px 14px`). Contains, in order: view segmented control,
   "Label" segmented control, "Colour" segmented control, search input (`flex: 1 1 200px`),
   "Function" select, flip button, Clear button.
3. **Filter bar** — a wrapping row of pill buttons: "PERIPHERALS" caption, one pill per
   peripheral (swatch + name + count), a 1px vertical divider, then the legend pills.
4. **Main** — flex row, wraps: the diagram card (`flex: 1 1 640px`, `border-radius: 12px`,
   `padding: 16px`, `overflow: auto`) and a `flex: 0 0 330px` sticky aside (`top: 16px`)
   holding the pin inspector card and the design-checks card.
5. **Workbench** — full-width card with a tab row (Mapping / JSON / C++ / Markdown), the
   mapping status line, "Reset to firmware", and a contextual Copy button.

### View: Nucleo board

Centred flex row: left connector group(s), the MCU block, right connector group(s). Each
connector is a labelled column — 10px uppercase mono title over a CSS grid
(`repeat(2, 104px)` for morpho, `112px` for the Arduino headers, `gap: 3px`).

MCU block: 126 × 126px, `background: oklch(0.27 0.012 255)`,
`border: 1px solid oklch(0.38 0.014 255)`, `border-radius: 8px`, a 7px pin-1 dot at top-left,
"STM32F446RE" (12px mono 600) over "LQFP64" (10px mono muted), with a 10px caption underneath.

### View: LQFP64 chip

CSS grid, `gridTemplateColumns: 140px repeat(16, 34px) 140px`,
`gridTemplateRows: 140px repeat(16, 26px) 140px`, `gap: 2px`. The package body spans
`grid-column: 2 / span 16; grid-row: 2 / span 16`.

Pad placement, pin 1 top-left, counter-clockwise:

| pins | column | row |
|---|---|---|
| 1–16 | 1 | n + 1 |
| 17–32 | n − 15 | 18 |
| 33–48 | 18 | 18 − (n − 32) |
| 49–64 | 18 − (n − 48) | 1 |

Left rail cells are `flex-direction: row`; right rail `row-reverse` + `text-align: right`.
Top and bottom rails are `flex-direction: column-reverse` / `column` with
`align-items: center` and `padding: 5px 2px`.

**Important:** on the vertical rails, `writing-mode: vertical-rl` goes on the **label span
only** (with `max-height: 104px`, `white-space: nowrap`, `overflow: hidden`,
`text-overflow: ellipsis`) — never on the flex container, which collapses the label to zero
inline size. The pin-number span drops its `min-width` on those cells.

### View: Pin table

Sticky-header table, `max-height: 70vh`, 12px body type. Columns: # · Pin · Type · Board ·
Used as · Alternate functions. Rows are filtered by the active match set, highlight on hover
(`oklch(0.255 0.013 255)`), and select on click (`oklch(0.27 0.02 195)`).

## Components

### Pin cell (the core primitive, used by all views)

A `<button>` with `data-pin="<base>"`, flex row, `gap: 6px`, `border-radius: 5px`,
`font-family: 'IBM Plex Mono'`, `transition: opacity .12s, box-shadow .12s, background .12s`.
Roomy density: `padding: 5px 8px`, `font-size: 12px`. Compact: `3px 6px` / `11px`.

Two children only: a pin-number span (9px, `oklch(0.58 0.012 255)`, `min-width: 15px`) and a
**single** label span (`font-weight: 600` when assigned, else 400, nowrap + ellipsis).

> The single label is a requirement, not an accident: the old component printed both the
> silkscreen name and the MCU name in every cell. What the label says is controlled globally
> by the Label switch — **Board** (silkscreen, e.g. `D13`), **MCU** (`PA5`), or **Signal**
> (the macro, or the role with any parenthetical stripped; falls back to the MCU name when
> unassigned). Everything else moves to the inspector and the native `title` tooltip
> (`Pin 21 · PA5 · Arduino D13 · CN10-11 · VS1053: SPI1_SCK | SD card: SPI1_SCK`).

Colour resolution, in order:

1. base colour = category colour (`power` / `ref` / `reset` / `boot` / `debug` / `io` / `nc`);
2. if assigned and colour mode is `category` → the "used" cyan; if mode is `peripheral` → that
   peripheral's hue; if mode is `conflict` → red for an error, amber for a warning, green when
   assigned and clean, grey otherwise;
3. `background: color-mix(in oklab, <colour> 22%, <page bg>)` when assigned, `9%` when not;
4. `border: 1px solid color-mix(… 62%/30% …)`;
5. text always `oklch(0.94 0.006 255)` — never tinted, so contrast holds on every ground.

States: selected → `box-shadow: 0 0 0 2px oklch(0.80 0.15 195)`; hovered → 1px of the same;
matched while a filter is active → `0 0 0 1px color-mix(… 80% …)`; unmatched while a filter is
active → `opacity: 0.22`.

### Segmented controls (view / label / colour / workbench tabs)

A 2px-padded track (`background: oklch(0.19 0.012 255)`, `border-radius: 7px`) holding buttons
of `padding: 6px 11px`, `border-radius: 5px`, 12px. Active: cyan fill `oklch(0.80 0.15 195)`,
text `oklch(0.19 0.03 195)`, weight 600. Inactive: transparent, `oklch(0.78 0.010 255)`.

### Filter pills (peripherals and legend)

`padding: 4px 9px`, `border-radius: 20px`, 11.5px mono, with an 8px square swatch. Inactive:
`oklch(0.235 0.014 255)` on a `oklch(0.32 0.014 255)` border. Active: `color-mix(… 26%)` fill,
`color-mix(… 75%)` border, text `oklch(0.96 0.006 255)`. Each shows a count.

### Pin inspector (aside, top card)

Header: "Pin 55" chip, the pin name at 19px mono 600, and a category badge tinted with the
category colour. Then a definition list of 78px uppercase labels against values: Board,
Default, I/O, Alternate, Board note (only when one exists), Extra (only when the pin has
additional functions).

Alternate functions are rendered as clickable chips (10.5px mono) — hover highlights that
function family across the diagram, click latches it into the Function filter.

"Assignments": one row per mapping row touching this pin, tinted with the peripheral colour,
showing peripheral, role and a `×` to unassign. Then the quick-assign row — peripheral select
(with "+ New peripheral…" revealing a name field), role, macro, Assign — and under it a row of
dashed suggestion chips built from the pin's own alternate functions (excluding `EVENTOUT`),
which fill the role field on click.

### Design checks (aside, bottom card)

One clickable row per finding (click selects the pin): the pin name in the severity colour,
then the message. Three checks only, by explicit request:

1. **Two peripherals on one pin.** Group mapping rows by pin. If the roles differ (comparison
   strips any parenthetical) → `error`: "Driven by A + B with different roles (x / y)". If the
   roles match → `info`: "Shared bus line: … — fine if they share SPI1_SCK". Also `error` when
   the pin name is not on the package, or when it is a power/reset/boot pin.
2. **Debug pins reused.** Any assigned pin whose category is `debug` or whose default matches
   `/JTDO|NJTRST|JTDI/` → `warn`. Catches PB3 (JTDO/TRACESWO) and PB4 (NJTRST) in the current
   firmware mapping.
3. **Bus signal on an incompatible pin.** Extract `[A-Z][A-Z0-9]*(_[A-Z0-9]+)+` tokens from the
   role, keep those matching a known peripheral family, and `warn` when the token is absent
   from that pin's function blob: "SPI2_SCK is not an alternate function of PB13".

Severity colours: error `oklch(0.78 0.13 25)`, warn `oklch(0.80 0.13 60)`, info
`oklch(0.72 0.13 250)`.

### Mapping editor (workbench, default tab)

`border-spacing: 0 4px` table, `max-height: 52vh`. Columns: Peripheral (text), Bus (text),
Pin (text input bound to a **single shared `<datalist>`** of the 60 MCU pin names — not a
`<select>` per row; 37 selects put >2000 option nodes on the page and made interaction
visibly laggy), Role (mono text), Macro (mono text), Check (the row's worst finding, coloured
by severity), and a `×` delete button. A row whose pin name is unknown gets a red field border
(`oklch(0.60 0.13 25)`). The selected pin's rows are tinted `oklch(0.27 0.02 195)`. Below the
table, a dashed "+ Add mapping row" button.

### JSON / C++ / Markdown tabs

JSON: a 16-row mono textarea with an Apply button and a status line (green-ish on success,
red on a parse error, with the parser message). C++ and Markdown are read-only `<pre>` blocks
with a Copy button that flips to "Copied" for 1.4s.

C++ export groups rows by peripheral, emits `#pragma once`, a `// Peripheral — Bus` comment
per group, and column-aligned `#define <MACRO> <PIN>` lines. The macro name is `row.macro`
when present, otherwise `PERIPHERAL_ROLE` uppercased with non-alphanumerics collapsed to `_`.

## Interactions

| Trigger | Effect |
|---|---|
| Hover a pin cell / table row / check row | 1px cyan ring on that pin everywhere |
| Click a pin cell / table row / check row / peripheral-card row | Selects it into the inspector |
| Hover a peripheral pill | Temporarily highlights that peripheral's pins |
| Click a peripheral pill | Toggles it into the multi-select filter |
| Hover a legend pill | Highlights that category |
| Click a legend pill | Latches that category until clicked again |
| Hover an alternate-function chip | Highlights every pin with that function family |
| Click an alternate-function chip | Latches it into the Function filter |
| Type in search | Matches pin name, Arduino/morpho name, function blob, peripheral, role, macro |
| Function select | Matches the family by regex |
| Clear | Drops search, function, peripherals and legend latch; the button itself turns cyan while any filter is active |
| Flip | Mirrors the layout horizontally |

Filters **combine with AND**. The result is a `Set` of matching pin bases; a null set means no
filter is active and nothing dims.

Flip: on the board, the right-side connector groups render on the left in reverse order and
vice versa, and the 2-column morpho grids swap their columns (`column = 2 - (i % 2)`). On the
chip, `column = 19 - column`, the pin-1 dot moves to the top-right, and the caption reads
"pin 1 top-right · clockwise". The button label toggles between "Top view" and "Bottom view"
and turns violet when flipped.

## State

```
rows          mapping rows (null until hydrated from localStorage)
view          'board' | 'chip' | 'table'
naming        'board' | 'mcu' | 'signal'
color         'category' | 'peripheral' | 'conflict'
density       'roomy' | 'compact'
flip          boolean
sel           selected pin base (default 'PB3')
hover         hovered pin base
legend        latched category id
periph        array of selected peripheral names
hoverPeriph   transient peripheral hover
fn            latched function family
hoverFn       transient function hover
q             search string
tab           'map' | 'json' | 'cpp' | 'md'
json, jsonErr JSON pane buffer and status
aPeriph, aNew, aRole, aMacro   quick-assign form
copied        copy-button feedback flag
```

### Performance requirements (learned the hard way)

The match set, the issue list and a pin→rows index must each be computed **once per render**
and handed to the cells; computing them inside the per-cell function meant ~113 × (37-row scan
+ regex construction) per render, which made a filter click take longer than a second and
appear not to work at all. Also memoise the family regexes in a map, and memoise the issue
list and the pin index against the mapping array's identity. In React terms: `useMemo` on
`pins`, `usedIndex`, `issues` and `matchSet`, and `React.memo` on the pin cell.

## Design tokens

```
Background        oklch(0.19 0.012 255)
Surface           oklch(0.225 0.013 255)   cards
Surface raised    oklch(0.235 0.014 255)   toolbar, aside, pills
Inset             oklch(0.17 0.012 255)    code blocks, inputs
Border            oklch(0.30 0.014 255)
Border strong     oklch(0.32–0.38 0.014 255)
Text              oklch(0.94 0.006 255)
Text muted        oklch(0.66 0.012 255)
Text faint        oklch(0.60 0.012 255)
Accent (cyan)     oklch(0.80 0.15 195)     on-accent text oklch(0.19 0.03 195)

Category — power  oklch(0.72 0.13 25)
           ref    oklch(0.72 0.13 95)
           reset  oklch(0.72 0.13 60)
           boot   oklch(0.72 0.13 310)
           debug  oklch(0.72 0.13 250)
           io     oklch(0.72 0.13 155)
           nc     oklch(0.50 0.010 255)
           used   oklch(0.80 0.15 195)

Peripheral hues   oklch(0.72 0.14 H), H ∈ [205, 25, 155, 285, 75, 330, 120, 250]
                  assigned by first-appearance order in the mapping

Radii             5px cells/inputs · 7px buttons/segments · 10px toolbar · 12px cards · 20px pills
Spacing           3 · 6 · 8 · 10 · 12 · 14 · 16 · 20 · 22px
Type              'Space Grotesk' — UI, headings
                  'IBM Plex Mono' — every pin name, signal, macro, count and code block
                  sizes 9 / 10 / 10.5 / 11 / 11.5 / 12 / 13 / 14 / 19 / 20 / 26px
Transitions       .12s on opacity, box-shadow, background, border-color
```

Colours are `oklch()` throughout and tints are `color-mix(in oklab, <colour> N%, <page bg>)` —
mixing toward the background rather than using alpha keeps text contrast predictable.

## Assets

None. No images, no icon font, no inline SVG — every glyph is a text character in one of the
two Google fonts (Space Grotesk 400/500/600/700, IBM Plex Mono 400/500/600).

## Files

- `Nucleo Pinout Studio.dc.html` — the design. Template (markup, inline styles) followed by the
  `Component` logic class holding `RAW`, `CONN`, `FW`, `FAMS`, `NOTES`, the derivations, the
  checks and the exporters.
- `support.js` — prototyping runtime only. **Do not port.**

Upstream sources for the data: STM32F446xC/E datasheet DS10693 (`datasheets/stm32f446re.pdf`),
Nucleo user manual UM1724 (`datasheets/nucleo.pdf`), and the firmware pin mapping in
`software/src/pinout.h`, `software/src/rendering/ILI9341_driver.h`, `software/src/main.cpp`.
