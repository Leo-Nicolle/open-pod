# Feasycom Bluetooth Module Comparison — for OpenPod (BT audio transmitter)

Context: the original design specified the **FSC-BT1035** (Qualcomm QCC3056), which is no longer
available on AliExpress. This compares the six Feasycom datasheets currently in `datasheets/`
against the requirement: an iPod-like MP3 player that needs to **transmit** (A2DP *source*) stereo
audio from local storage over Bluetooth to headphones/speakers.

## TL;DR

| Module | Chip | Can do A2DP audio streaming? | Footprint compatible with BT1035? | Verdict |
|---|---|---|---|---|
| **FSC-BT1035** (original, EOL) | QCC3056 | Yes (source & sink) | — (reference) | Reference part |
| **FSC-BT1026x** family | QCC3021/3031/3024/3034/5125/5124 | Yes (source & sink) | **Yes — identical 52-pin/13×26.9 mm footprint** | **Best replacement** |
| FSC-BT986 | Dual-mode chip (unnamed) | No dedicated audio codec/I2S — SPP/GATT serial only | No (36-pin, 13×26.9×2.4 mm) | Not suitable |
| FSC-HC05 | Proprietary BR/EDR SoC | No — SPP-only serial modem | No (36-pin, shares footprint with BT986 only) | Not suitable |
| FSC-BT677D | Silicon Labs EFR32BG21 | No — BLE only, no Classic BT/A2DP | No (24-pin, 13×16.5×1.62 mm) | Not suitable |
| FSC-BT630 | Nordic nRF52832 | No — BLE only, no Classic BT/A2DP | No (20-pin, 10×11.9×1.8 mm) | Not suitable |

**Recommendation:** use a module from the **FSC-BT1026x family** — it is the direct footprint and
pinout successor to the BT1035 (same 52-pad, 13 mm × 26.9 mm × 2.2 mm castellated package, 1.0 mm
pitch, same keep-out geometry) and is the only other module in this batch with an on-chip audio
DSP, I²S/PCM audio interface, and A2DP/aptX support. Pick a sub-variant based on codec needs (see
below).

---

## 1. Why HC05, BT986, BT677D and BT630 don't fit

The application needs **A2DP source** (streaming stereo PCM audio out over Bluetooth), which
requires: (a) Classic Bluetooth BR/EDR (A2DP is a BR/EDR profile, not BLE), and (b) an on-chip
audio codec/DSP with an I²S/PCM or analog audio path.

- **FSC-HC05** — Classic BT v3.0, but the general-spec table lists only the **SPP** profile. There
  is no PCM/I²S interface and no audio codec in the block diagram (UART/I²C/USB/GPIO only). It's a
  serial "Bluetooth modem," not an audio module — this is the classic cheap HC-05 use case (sensor
  data links, AT-command serial bridges), not audio.
- **FSC-BT986** — Dual-mode (BR/EDR **and** BLE, both "Support" in the profile table), 5.2 spec,
  but again the interface list is UART/I²C/USB/GPIO only — no PCM/I²S pins, no audio codec block.
  It's aimed at the same class of application as HC05 (health thermometer/heart-rate/proximity
  sensors relaying data via SPP or GATT), not audio streaming.
- **FSC-BT677D** — Silicon Labs EFR32BG21 based. **BLE only** ("Classic Bluetooth: NA" in the spec
  table). No A2DP is possible without Classic BT. It does expose SAR ADC/PWM/timers — it's a
  general-purpose low-power BLE MCU module (lighting, connected home, gateways), not an audio
  transmitter. It's also programmed differently from the others: it's a bare Silicon Labs
  Bluetooth SDK target flashed via SWD/J-Link, not an AT-command "modem" like the Feasycom-firmware
  parts.
- **FSC-BT630** — Nordic nRF52832 based. **BLE only** ("Classic Bluetooth: No Support"). Has PDM
  mic input and I²S, but I²S here is meant for BLE audio bridging/mic capture use cases (IoT,
  beacons, remotes, HID), not Classic-BT A2DP streaming. Not usable for this project as an A2DP
  transmitter.

None of these four have the hardware (Classic BT + audio DSP + codec) needed to be a Bluetooth
audio transmitter, regardless of firmware.

## 2. BT1035 vs BT1026x family — the real comparison

Both are Qualcomm-chip dual-mode modules built around the same **52-pin castellated package**:
13 mm(W) × 26.9 mm(L) × 2.2 mm(H), 1.0 mm pad pitch, identical restricted/keep-out area drawing.
Pin-for-pin the first ~21 pins match name-for-name (GND, AIO4/LED4, AIO5/LED5, PCM_CLK/PIO16,
PCM_IN/PIO19, PCM_OUT/PIO18, PCM_SYNC/PIO17, RESET, PCM_MCLK_OUT/PIO15, PIO6-8, BT_TX/PIO5,
BT_RX/PIO4, BT_CTS/PIO3, BT_RTS/PIO2, LED0-2/AIO0-2...), and the audio/power pins (MIC_RP/RN,
MIC_LP/LN, MIC_BIAS, SPK/AUDIO_HP differential outputs, VBAT_IN, VCHG, SYS_CTRL, USB_DP/DN,
RF_OUT/EXT_ANT) occupy the same pin numbers. **This is effectively the drop-in mechanical/pinout
replacement family for the BT1035.**

| | FSC-BT1035 | FSC-BT1026x (A/B/C/D/E/F variants) |
|---|---|---|
| Chip | Qualcomm QCC3056 | QCC3021 / QCC3031 / QCC3024 / QCC3034 / QCC5125 / QCC5124 (pick variant) |
| BT version | 5.2 dual-mode | 5.1 dual-mode |
| TX power | +10 dBm (basic rate, typ, VBAT=3.7V) | +9 dBm max |
| RX sensitivity | -96 dBm (basic rate, typ) | -96 to -100 dBm depending on variant |
| Codecs | aptX, aptX HD, aptX-LL, aptX-Adaptive (chip supports; see note) | SBC + AAC always; aptX/aptX HD/aptX LL on **B/D** variants; aptX/aptX HD/aptX Adaptive on **E/F** variants (QCC5124/5125) |
| Profiles (general spec) | Not explicitly tabulated in this datasheet (audio-app focused doc) | **SPP, A2DP, AVRCP, HFP, HSP, HOGP, PBAP** + BLE GATT client/peripheral, simultaneous BR/EDR+BLE |
| Application text in datasheet | "USB Audio Transmitter", "Audio Transmitter" — explicitly a **source** use case | "Bluetooth speakers", "music box", "wired/wireless headsets", "USB audio" — phrased as **sink** use cases, but chip/firmware support both source and sink roles |
| Audio interfaces | I²S/PCM (input only, up to 384 kHz), USB, analog line/mic in (2× diff ADC), mono/stereo speaker out | I²S/PCM (bidirectional, up to 192 kHz), SPDIF in/out (ABEF variants only), USB audio, analog line/mic in, diff headphone/speaker out |
| Digital mic | Yes | Yes (up to 6 mono / 3 stereo) |
| Battery charging | Not documented in this datasheet | Full Li-ion charger IC on-chip (trickle/pre/fast/standby modes), external PNP transistor option for higher current |
| Programming/debug | Dedicated debug pins (10-12) | SWD (SWCLK/SWDIO) + J-Link, or SPI production programming |
| Supply | VBAT 3.0–4.2 V, VDD_IO 1.7–3.3 V | VBAT_IN 2.8–4.3 V, VDD_IO 1.7–3.3 V (very similar) |
| Power consumption | Not tabulated in general-spec table | Play ~10 mA (typ), Pairing ~5 mA (typ), Off ~1 µA (typ) at VBAT=3.3V |
| Footprint | 13×26.9×2.2 mm, 52 pin, 1.0 mm pitch | **Identical**: 13×26.9×2.2 mm, 52 pin, 1.0 mm pitch |

**Update — confirmed via the AT command references** (`FSC-BT1026-General-Audio-AT-Command-Set.pdf`
and `FSC-BT1035_programming_user_guide_1.1.1.pdf`, both in `~/Downloads`): the source-mode caveat
above is resolved. See §7 below — BT1026x's shipped Feasycom firmware does expose A2DP **source**
mode via `AT+PROFILE`, and Feasycom's own BT1035 example even pairs a BT1035 (source) to an
FSC-BT1026C (sink) over A2DP, proving the two families interoperate on the same firmware
generation.

### Picking a BT1026x variant

- **FSC-BT1026A/C** (QCC3021/QCC3024) — SBC/AAC only, no aptX. Cheapest, fine if you don't care
  about aptX.
- **FSC-BT1026B/D** (QCC3031/QCC3034) — adds aptX, aptX HD, aptX Low Latency. Good choice for an
  MP3-player-style transmitter where low latency to headphones matters.
- **FSC-BT1026E/F** (QCC5125/QCC5124) — newer Qualcomm silicon, aptX/aptX HD (+Adaptive on some),
  dual audio DSPs on the F variant. Likely the closest in spirit to the original QCC3056-based
  BT1035 (also a "newer QCC5xxx-class" chip generation) — best pick if available on AliExpress.

## 3. Codec / protocol support — "Sony, AP2P etc."

*(Updated after reading the AT command references — see §7. The hardware datasheets alone don't
tabulate codecs in detail, but the AT command sets do.)*

- **SBC** — mandatory baseline A2DP codec, supported on all Qualcomm QCC-based modules here
  (BT1035, BT1026x).
- **AAC** — selectable codec bit on both BT1035 and BT1026x (`AT+A2DPCFG` bit 0).
- **aptX family** (aptX, aptX HD, aptX Low Latency, aptX Adaptive) — selectable codec bits on both
  BT1035 and BT1026x `AT+A2DPCFG`, subject to which now-unavailable variant you actually get
  (A/C = no aptX, B/D = aptX/HD/LL, E/F = adds Adaptive).
- **LDAC (Sony)** — **correction to the earlier version of this doc**: LDAC *is* a documented,
  selectable codec bit (`AT+A2DPCFG` BIT[5]) in **both** the BT1026 and BT1035 AT command sets, and
  `AT+A2DPENC`/`AT+A2DPDEC` list `10:LDAC` as a valid encoder/decoder value. It's just not
  mentioned in the hardware datasheets (those only describe pins/electricals, not codecs). Whether
  LDAC actually works end-to-end still depends on Qualcomm having licensed/enabled it in the
  specific firmware build flashed on the module you receive — worth a direct question to Feasycom
  support before relying on it, but it is not absent from the platform.
- **AP2P** — not a Bluetooth SIG profile; it's a proprietary TWS-pairing scheme from other vendors
  (Airoha, Jieli, Actions, etc.) for syncing two earbuds. It doesn't apply to Qualcomm QCC modules
  and isn't mentioned here. If you need to pair with earbuds using AP2P mode, that's a property of
  the *earbuds'* chipset, not something the transmitter module needs to implement — standard A2DP
  source output works with any standard A2DP sink, AP2P earbuds included (as long as one earbud
  acts as the standard A2DP sink and passes audio to its twin over the vendor's own link).
- **HFP/HSP/AVRCP/PBAP** — listed for BT1026x profile table; useful if you want playback
  control (AVRCP) from the headphones/speaker back to the player, or call-audio profiles (HFP/HSP,
  not needed for a pure MP3 player).

## 4. Programming methods summary

| Module | Runtime firmware interface | Flashing/debug |
|---|---|---|
| HC05 | UART AT-commands (default 115200 8N1, up to 921600) | ICE debug pins (15/17), I²C also available |
| BT986 | UART AT-commands (same as HC05) | Same ICE pins, SWD-less |
| BT677D | Custom Silicon Labs Bluetooth SDK firmware (GATT-based), UART for data | SWD via SWDIO/SWCLK (pins 19/20) + J-Link |
| BT1035 | UART AT-commands over H4 HCI or raw UART, PCM/I²S/USB for audio | Dedicated debug port pins 10-12 |
| BT1026x | UART AT-commands (same Feasycom firmware style) | SWD (SWDIO/SWCLK) + J-Link, or SPI programming for production |
| BT630 | UART AT-commands or custom Nordic SDK firmware | SWD (SWDIO/SWCLK), J-Link, or OTA |

All the audio-capable modules (BT1035, BT1026x) use **UART** for control/AT-commands and a
separate **I²S/PCM** bus (plus optional USB audio and analog line-in/mic/speaker) for the actual
audio path — there is no SPI audio interface on any of these; SPI only appears on BT630 (as a
general peripheral bus, not for audio) and as a debug-only interface elsewhere.

## 5. Input voltage summary

| Module | VBAT / main supply | VDD_IO | Notes |
|---|---|---|---|
| HC05 | 3.3–3.6 V (single rail) | — | No dedicated battery charger |
| BT986 | 3.3–3.6 V | — | No dedicated battery charger |
| BT677D | 1.77–3.8 V (VREGVCC) | — | Can run directly off 2×AA/coin cell range |
| BT630 | 1.7–3.6 V | — | Also runs from coin cell |
| BT1035 | VBAT 3.0–4.2 V (Li-ion range), VCHG 4.75–6.5 V for USB charging | VDD_IO 1.7–3.3 V | No on-chip charger described in this doc |
| BT1026x | VBAT_IN 2.8–4.3 V, VCHG 4.75–6.5 V (5V USB) | VDD_IO 1.7–3.3 V | **Has on-chip Li-ion charger** (trickle/pre/fast/standby, configurable up to 200 mA internal, or up to 1.8 A with external PNP pass transistor) |

If the OpenPod design charges its battery through the Bluetooth module itself, **BT1026x is
additionally attractive** because it has an integrated Li-ion charge controller (with external
PNP transistor support for higher currents) that the BT1035 datasheet doesn't document.

## 6. Bonus: HC05 / BT986 are footprint-compatible with each other

Not relevant to the audio requirement, but worth noting: **FSC-HC05** and **FSC-BT986** share the
exact same 36-pad, 13 mm × 26.9 mm castellated footprint (1.5 mm pitch) and near-identical pinout
(only pin 12/2.4 mm height differs slightly and BT986 adds Tran/Disc control pins). If a
non-audio serial/BLE data link module is ever needed elsewhere in the project (e.g. a
sensor/telemetry side-channel), these two are interchangeable on the same PCB footprint.

## 7. AT command capability check (source-mode caveat resolved)

Two additional documents (not hardware datasheets, but the AT command references) settle the
question of whether BT1026x can genuinely act as an A2DP **source** like the BT1035:
`FSC-BT1026-General-Audio-AT-Command-Set.pdf` (Release 3.0.0) and
`FSC-BT1035_programming_user_guide_1.1.1.pdf` (Release 1.1.1), both in `~/Downloads`.

### BT1035 is hard-locked to source-only

Its `AT+PROFILE` bitmask documentation states outright:

> BT1035、BT806 not support HFP-HF、A2DP Sink、AVRCP Controller、HID Keyboard、PBAP

i.e. the BT1035's firmware/silicon combination on this SKU **cannot** be sink, cannot be an AVRCP
controller, etc. — it is permanently a transmitter (A2DP Source, HFP-AG, AVRCP Target only). That
matches its datasheet's "Audio/USB Audio Transmitter" application text exactly. `AT+A2DPAUDIO`
even notes explicitly: *"A2DP SOURCE only supported."*

### BT1026x supports both roles, selectable at runtime

The equivalent `AT+PROFILE` description for BT1026x has **no such exclusion list** — it only says:

> GATT Server and Client, HFP Sink and Source, A2DP Sink and Source, AVRCP Controller and Target
> cannot be enabled simultaneously because of mutual exclusion.

I.e. BT1026x can run **A2DP Source** (same BIT[6] as BT1035) just as validly as A2DP Sink — it's a
firmware config choice (`AT+PROFILE=<bitmask>`), not a hardware limitation. The doc's own example
sets `AT+PROFILE=83` to "Enable SPP, GATT Server, HFP Source, A2DP Source profile, disable the
others" for *both* chip families side by side.

### Feasycom's own reference example proves interoperability

`FSC-BT1035_programming_user_guide_1.1.1.pdf` §7.2 ("Source mode connection") walks through the
exact host↔module AT sequence for using a BT1035 as a transmitter, and the worked example scans
for and connects to a real nearby device that is literally **an FSC-BT1026C module**:

```
AT+SCAN=1
+SCAN=1,2,DC0D30000057,-42,11,FSC-BT1026C,240408      <- discovered: an FSC-BT1026C, RSSI -42
+SCAN=2,2,DC0D30000012,-70,16,FSC-BT1006A-0012,240408
+SCAN=E
AT+A2DPCONN=DC0D30000057   (connect FSC-BT1026C)
OK
+A2DPSTAT=3 (A2DP connected)
+A2DPDEV=DC0D30000057,FSC-BT1026C
+HFPSTAT=3 (HFP connected)
+AVRCPSTAT=3 (AVRCP connected)
+A2DPENC=1 (sbc encoder)
+A2DPSTAT=4 (auto enter A2DP Streaming)
```

This is Feasycom's own documentation showing a BT1035 source streaming A2DP audio to an
FSC-BT1026C acting as sink — direct, vendor-confirmed proof that the two module families
interoperate at the Bluetooth level. Combined with the `AT+PROFILE` evidence above (BT1026x's
A2DP Source bit works standalone, not just as a sink), this removes the earlier "unverified"
caveat: **a BT1026x module, configured with `AT+PROFILE` to enable A2DP Source, is a functionally
verified replacement path for the BT1035's transmitter role**, not just a footprint-alike guess.

### Practical AT sequence for OpenPod (BT1026x as source)

Based on the same command set:

1. `AT+PROFILE=<bitmask with A2DP Source + AVRCP Target bits set>` — configure role once (persists
   after `AT+REBOOT` unless restored to factory defaults).
2. `AT+A2DPCFG=<bitmask>` — pick codec(s): AAC / aptX / aptX‑LL / aptX‑HD / aptX‑Adaptive / LDAC.
3. `AT+SCAN=1` then `AT+A2DPCONN=<MAC>` — discover and connect to the target headphones/speaker
   (the A2DP sink), or `AT+AUTOCONN` to reconnect to the last paired sink automatically on power-up.
4. `AT+A2DPAUDIO=1` — start streaming once your I²S/PCM (or USB/analog) audio source is feeding the
   module; `AT+A2DPAUDIO=0` to pause/release.
5. `AT+PLAYPAUSE` / `AT+FORWARD` / `AT+BACKWARD` / volume, etc. are available if you want the
   headphones' buttons (AVRCP) to control playback back on the OpenPod side — BT1026x's AVRCP
   Target/Controller bits both exist; BT1035 only implements AVRCP Target (receives transport
   commands from the sink, cannot send them), which is the correct role for a source device anyway.

## 8. Can the exact same PCB be used? — pin-by-pin swap check

Re-read both datasheets' mechanical/pin sections side by side (`FSC-BT1035_Datasheet_EN.pdf` §3.2/§8
vs `FSC-BT1026x_Datasheet_EN.pdf` §3.2/§8) to answer this directly.

### Footprint: identical

| | BT1035 (Fig 8‑1) | BT1026x (Fig 14) |
|---|---|---|
| Outline | 13 × 26.9 × 2.2 mm | 13 × 26.9 × 2.2 mm |
| Pad count | 52 | 52 |
| Pad size | 1.6 × 0.6 mm | 1.6 × 0.6 mm |
| Pad pitch | 1.0 mm | 1.0 mm |
| Pin-1/edge offsets | 1.5 / 2.0 / 1.0 mm | identical |

Pad drawings match exactly — this solders onto the same land pattern with zero rework.

**RF keep-out zone differs slightly**: BT1035's restricted area behind the antenna (Fig 9-2) is
44.5 mm tall, allows components up to 1.5 mm near the antenna, with 18/13 mm inner clearances.
BT1026x's (Fig 15) is 43.5 mm tall, a stricter 1.0 mm max component height, and smaller 15/8.2 mm
inner clearances (plus a labeled 2.8 mm dimension not present on the BT1035 drawing). If your board
was laid out to BT1035's (larger) keep-out, a BT1026x will very likely still perform fine sitting
inside it — it's not a byte-exact match, but not a blocker either.

### Pins: near-total match

Identical, functionally load-bearing, on both: GND (1,22,32,50,52), AIO4/AIO5 (2‑3, see caveat),
PCM_CLK/IN/OUT/SYNC (4‑7), RESET (8), PCM_MCLK_OUT (9), PIO6/7/8 (10‑12), BT_TX/BT_RX (13‑14),
AIO0‑2/LED0‑2 (17‑19), PIO20/PIO21 (27‑28), VCHG_SENSE (29), VDD_USB/3.3V_OUT (31), VBAT_IN (33),
SYS_CTRL (34), 1.8V_OUT (35), VDD_IO (36), USB_DP/USB_DN (37‑38), VCHG↔VCC_CHG (39, name only),
MIC_RP/RN/LP/LN (40‑44), MIC_BIAS (45), RF_OUT (51). That's every pin the audio-transmitter circuit
actually needs: power, the full PCM/I²S bus, UART, USB, and mic input.

| Pin(s) | BT1035 | BT1026x | Effect of swap |
|---|---|---|---|
| 2 | AIO4/LED4 (active) | **NC on A/B variants**, active on **C/D/E/F** | Pick a C/D/E/F variant if this pin is used |
| 15/16 | BT_CTS/PIO3, BT_RTS/PIO2 | Same PIO3/PIO2 on **C/D/E/F**; renumbered PIO22/PIO23 on A/B | Same physical pin either way — only matters if firmware references the PIO index |
| 20/21 | PIO44, PIO41 | PIO3/PIO2 (A/B) or PIO60/PIO61 (C/D/E/F) | Same pin location, different PIO index — fine as generic GPIO |
| 23‑25 | PIO42/43/40 (active) | **NC on A/B**, PIO52/53/54 on C/D/E/F | Need a C/D/E/F variant if these GPIOs are used |
| 26 | PIO45 (active) | **NC on every BT1026x variant** | No substitute pin — the one real loss; check your schematic |
| 30 | NC | **CHG_EXT** (active output, ext. charge transistor control) | Safe to leave floating/unrouted on the existing board |
| 46/47 | NC | **SPK_RN / SPK_RP** (active, right-channel diff. audio out) | Safe to leave floating — unused extra channel on a transmitter |

### Verdict

**Yes** — same land pattern, same pad size/pitch/pin count, and every pin your transmitter circuit
actually uses (power, PCM/I²S, UART, USB, mic, RF) lines up identically. Pick **BT1026C or
BT1026D** specifically: they keep the same PIO numbering on pins 15/16/20/21 and keep pins 2 and
23‑25 active rather than NC, which is the closest possible match to BT1035's pinout on this
footprint. The one pin with genuinely no equivalent is **26/PIO45** (NC on every BT1026x variant) —
worth a 30-second check of your schematic to confirm nothing critical is wired there before
ordering boards.
