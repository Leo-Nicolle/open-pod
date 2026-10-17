# VS1053b: Equalizer options

Can the opentone have an equalizer, given that the VS1053b already runs the VLSI FLAC plugin?

**TL;DR:** We can't fit a real multi-band EQ plugin next to the FLAC plugin. The chip's built-in bass/treble control costs no memory, though, and is the practical way to offer an EQ.

## 1. Built-in bass/treble control (`SCI_BASS`)

The VS1053b has a simple tone control in ROM, driven by the `SCI_BASS` register (0x02). It needs no plugin and costs no instruction RAM. It keeps working with the FLAC plugin loaded.

| Bits | Name | Meaning |
|---|---|---|
| 15:12 | `ST_AMPLITUDE` | Treble level, −8…7 in 1.5 dB steps (−12 dB … +10.5 dB). 0 = off |
| 11:8 | `ST_FREQLIMIT` | Treble lower corner, 1…15 kHz (×1 kHz) |
| 7:4 | `SB_AMPLITUDE` | Bass enhancement, 0…15 dB (0 = off) |
| 3:0 | `SB_FREQLIMIT` | Bass upper limit, 2…15 (×10 Hz → 20…150 Hz) |

The bass enhancer is an "enhancer" rather than a plain shelf boost. The datasheet recommends a cutoff near the lowest frequency the speakers or headphones can reproduce, to avoid clipping.

State in the code:
- `VS1053_REG_BASS` is already defined in `software/src/sound/VS1053_driver.h:17`.
- Nothing writes to it yet. Adding the feature means one `writeRegister(VS1053_REG_BASS, value)` call plus a settings menu (e.g. Bass / Treble presets).

## 2. A full multi-band EQ plugin: no room next to FLAC

### Memory used by the FLAC plugin

`software/src/sound/VS1053_FLAC_plugin.h` holds VLSI's FLAC + patches image: 8208 SCI writes (`atab`/`dtab`). I decoded its `SCI_WRAMADDR` / `SCI_WRAM` runs to see which memory it loads:

| Region | Loaded by the FLAC plugin | User-available on VS1053b |
|---|---|---|
| Instruction RAM | 0x0050–0x0F25 (≈ 3,800 words) | 0x0050–0x0FFF (≈ 4,000 words) |
| Instruction RAM (vectors) | 0x28, 0x2E, 0x30, 0x32 | — |
| X RAM | 0x180B (+88 words), plus a block at 0x3580 (56 words) | small |
| Y RAM | 0x1800 (+32 words) | small |

There is also a 210-word run whose `WRAMADDR` is 0xFAB3, which is outside the documented user I-RAM range. I haven't looked at what it is.

That leaves about **218 instruction words** free (0x0F26–0x0FFF), which is far too few for an equalizer plugin. VLSI ships its plugins as separate images that load into the same user I-RAM, so loading another one would overwrite part of the FLAC decoder.

### CPU headroom

FLAC already runs the chip at `SCI_CLOCKF = 0xC000` (4.5× → ≈ 55 MHz, the chip's maximum, see `configureFLACClock()`). That leaves little spare DSP time for extra filtering per sample, even if the code fit.

## 3. Alternatives for a "real" EQ

| Option | Notes |
|---|---|
| **Use `SCI_BASS`** (recommended) | Free. Two bands (bass + treble). Enough for "Bass boost / Treble boost / Flat" presets |
| EQ on the MCU | Would need to decode FLAC on the MCU, apply biquads, then send PCM/WAV to the VS1053b. A big architecture change, with heavy MCU and SPI load |
| Switch to **VS1063a** | Has a 5-band EQ in ROM and native FLAC decoding with no plugin, but it's a hardware change |

## Side note

`software/src/sound/FLAC_README.md` still says the plugin data is a "placeholder". That's out of date: the header contains the full 8208-word VLSI image.
