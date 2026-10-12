# VLSI VS10xx Codec Comparison — VS1053b vs VS1063a vs VS1073a

Context: OpenPod currently uses the **VS1053b** for MP3/audio decoding. This compares it against
the two newer pin-compatible parts in the same family, VS1063a and VS1073a (datasheets in
`datasheets/`, prefixed `vs*`), plus VLSI's own 1-page comparison chart
(`datasheets/vs10xx-comparison.pdf`).

## TL;DR

**Both VS1063a and VS1073a are pin-compatible drop-ins for VS1053b** — same LQFP-48 package, and
the full pin tables in all three datasheets are identical, pin-for-pin, pad name for pad name.
VLSI's own text confirms it:

> "VS1063a is a pin-compatible alternative for VS1053... has all the functionality of VS1053
> (except MP1 and MIDI decoding)"

> "VS1073a is a pin-compatible alternative for VS1053 and VS1063 with a change of core voltage
> regulator... has all the functionality of VS1063 (except AEC)"

| | VS1053b (current) | VS1063a | VS1073a |
|---|---|---|---|
| Pinout | reference | **identical** | **identical** (but different CVDD voltage, see §5) |
| Formats | baseline | +FLAC (ROM), −MIDI, −MP1 | ++FLAC/ALAC/APE/AC-3/AIFF/DSD/Opus (all ROM), −MIDI, −AEC (MP1 comes back) |
| Power | baseline | same as VS1053b | not a clear win — see §2 |
| DAC/ADC quality | baseline | marginally better THD | marginally better THD |
| RAM for user code | ~0.5+ KiB data RAM | up to 80 KiB data RAM | 32 KiB instruction RAM (2×) + up to 80 KiB data RAM |
| Status | mature, in production | mature | **preliminary datasheet, very new part** |

---

## 1. Formats: what you gain / lose

| | VS1053b | VS1063a | VS1073a |
|---|---|---|---|
| Decoders (ROM) | MP3, MP2, MP1, WAV, WMA, Ogg Vorbis, AAC, MIDI | MP3, MP2, WAV, WMA, Ogg Vorbis, AAC, **FLAC** | MP3, MP2, **MP1**, WAV, WMA, Ogg Vorbis, AAC, FLAC, **ALAC, Monkey's Audio (APE), AC-3, AIFF, DSD, Ogg Opus** |
| Decoders needing a loaded patch | FLAC | ALAC | — (everything above is in ROM, no patch needed) |
| Lost vs. VS1053b | — | **MP1, MIDI** | **MIDI** (MP1 comes back), **AEC** (echo cancel, lost vs. VS1063a) |
| Encoders (ROM) | ADPCM, PCM | ADPCM, PCM, Ogg Vorbis, MP3, g.711, g.722 | same as VS1063a **+ FLAC** |
| Recording (ADC) | Stereo line / mono mic | Stereo line / mono mic | Stereo line / mono mic |

For an MP3/audio-player-transmitting-to-Bluetooth use case:

- **VS1063a** — loses MIDI (General MIDI playback, unlikely to matter) and MP1 (rare MPEG
  layer-I audio, unlikely to matter). Gains FLAC decoding built into ROM (no patch juggling at
  boot), plus real recording/encoding to MP3/Ogg Vorbis/PCM if the device should ever record too.
- **VS1073a** — everything VS1063a has, minus AEC (acoustic echo cancellation — irrelevant for a
  music player, that's for speakerphone use), plus MP1 comes back, plus a lot more decoders
  (ALAC/Apple Lossless, FLAC, DSD, Opus, AC-3, Monkey's Audio) — all in ROM, no patches needed.
  This is the one to pick if lossless/hi-res playback (FLAC/ALAC/DSD) directly from the SD card
  matters.

## 2. Power consumption

Datasheet power tables, Ogg Vorbis playback at full volume (128 kbit/s for VS1053b/VS1063a,
110 kbit/s for VS1073a), ~25°C:

| | VS1053b | VS1063a | VS1073a |
|---|---|---|---|
| CVDD voltage | 1.8 V | 1.8 V | **1.25 V** |
| AVDD current (30/33 Ω load) | 11 mA | 11 mA | 16–21 mA (depends on VREF setting) |
| CVDD current | 11 mA | 11 mA | **5 mA** |
| Approx. total supply power | ~56 mW | ~56 mW | ~60–76 mW |

The "CVDD current 5 mA vs 11 mA" number looks like a big win, and digital-core power really is
roughly 3× lower on VS1073a thanks to the lower 1.25 V core voltage — but the analog output stage
(AVDD) draws *more* current in VS1073a's own test condition, so total system power ends up roughly
a wash to slightly worse in this specific test, not a clear win. Treat "VS1073a is lower power" as
**not proven** by these numbers alone — the DAC/analog side dominates total power either way, and
it didn't improve there. If power budget matters a lot, VS1063a is the safer bet (identical power
numbers to the current VS1053b, just a more capable chip), while VS1073a's benefit is format
support, not power.

**Caveat**: the VS1073a datasheet is watermarked "PRELIMINARY" and dated 2026-05-26 — a very
recently introduced (possibly not-yet-fully-validated or barely-released) part. Numbers could
still move; worth checking current sourcing/stock and maybe waiting for a non-preliminary revision
before committing.

## 3. DAC/ADC quality

Essentially a wash — VS1063a/VS1073a are marginally better than VS1053b, nothing regresses:

| | VS1053b | VS1063a | VS1073a |
|---|---|---|---|
| DAC resolution | 18-bit | 18-bit | 18-bit |
| THD | 0.07% | **0.04%** | **0.04%** |
| 3rd harmonic distortion | 0.02% | **0.01%** | **0.01%** |
| Dynamic range (IDR) | 100 dB | 100 dB | 100 dB |
| SNR | 94 dB | 94 dB | 94 dB |
| Mic SNR | 70 dB | 72 dB | 72 dB |
| Line-in SNR/THD | 90 dB / 0.014% | 90 dB / 0.014% | 90 dB / 0.014% |
| Headphone drive | 30 Ω capable | 30 Ω capable | 30 Ω capable |

Not something you'd hear a difference from in practice, but nothing to lose either.

## 4. RAM / extensibility

This is a bigger practical difference than the raw numbers suggest:

- **VS1053b**: 16 KiB instruction RAM, only **~0.5+ KiB** free data RAM for user code/patches.
- **VS1063a**: 16 KiB instruction RAM, **up to 80 KiB** data RAM (default ~4 KiB free, expandable).
- **VS1073a**: **32 KiB** instruction RAM (double VS1063a), up to 80 KiB data RAM.

VS1053b's tiny data RAM is exactly why FLAC needs a loaded patch on it, and things get tight if
DSP effects are wanted too. VS1063a/VS1073a essentially remove that ceiling — much more headroom
for custom plugins, patches, or user DSP code running alongside the decoder.

## 5. Other feature adds (VS1063a and VS1073a only, not on VS1053b)

- **AD Mixer** — monitor the analog input while a stream is playing.
- **PCM Mixer** — mix a side-stream over the main decoded stream.
- **Adjustable Speed Shifter** — time-stretch/pitch playback speed.
- **5-channel equalizer** (alternative to bass/treble).
- **I2S output width**: VS1053b/VS1063a output 16-bit I2S; **VS1073a goes to 16/32-bit**.
- **Full-duplex codec mode** (simultaneous encode+decode) on both — VS1063a additionally has AEC
  on top of it, VS1073a doesn't.

## 6. Hardware-level catch: the CVDD regulator

Pinout is identical across all three, but **VS1073a needs a different CVDD voltage (1.25 V) vs
VS1053b/VS1063a's 1.8 V**. VLSI's own comparison note is explicit about this:

> "While VS1003b can run from a single 2.8V power supply, using a separate CVDD regulator is
> highly recommended. With it you can populate the board with a different CVDD regulator and
> VS1053/63/73 or future IC that uses the same pinout (but newer technology). Use a 12.288MHz
> crystal, the LQFP-48 pinout from the VS1073a datasheet, and a separate CVDD regulator for your
> design to be future-proof in hardware."

Design the CVDD regulator as a separately-populated part (not hardwired to a fixed 1.8V feedback
network) to keep the board able to accept any of VS1053b/VS1063a/VS1073a interchangeably. If the
current OpenPod board hardwired an 1.8V CVDD regulator assuming VS1053b, dropping in a VS1073a
as-is would run its core out of spec — the fix is changing that one regulator (or its feedback
resistors) to produce 1.25V; everything else on the board (footprint, all other pins, crystal,
SPI/UART wiring) stays the same.

## 7. Bottom line

- **VS1063a** — closest thing to a strict upgrade with zero hardware changes: same CVDD voltage,
  same power numbers, same DAC quality (slightly better THD), gains FLAC-in-ROM and real
  recording/encoding, loses only MIDI and MP1 (both unlikely to matter for an MP3 player). Very
  low-risk swap.
- **VS1073a** — much broader format support (FLAC/ALAC/DSD/Opus/AC-3, all in ROM) and double the
  instruction RAM, at the cost of needing to re-spec the CVDD regulator (1.25V instead of 1.8V)
  and no clear power win over VS1063a. Also currently a preliminary/very-new part — check
  availability before designing it in.
