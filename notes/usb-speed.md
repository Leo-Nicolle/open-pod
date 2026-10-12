# USB: speed vs MCU, pins, complexity and debug

What USB has to do on OpenPod, what each hardware option costs, and the
recommendation.

## What USB has to do

One USB-C port, three roles:

| Role | How | Extra hardware |
|---|---|---|
| **Mass storage** | USB MSC class: the computer sees the SD card as a disk (native on Windows, macOS, Linux) | None |
| **Serial** | CDC-ACM in the same composite device (TinyUSB). Chrome's Web Serial can then talk to it from the website (logs, settings) | None |
| **Programming** | The STM32 ROM bootloader does USB DFU on PA11/PA12 (F446 and U5A5). Works with `dfu-util`, STM32CubeProgrammer and WebDFU (the "flash from the website" TODO) | BOOT0 on a button |

Entering DFU:

- **Normal path:** a "Firmware update" menu entry sets a flag in RAM, resets,
  and jumps to the system bootloader before anything else starts.
- **Recovery path:** BOOT0 wired to a button, so holding it while plugging in
  or resetting enters DFU. Needed because a broken firmware can't reach the
  menu.

Keep, whatever the USB choice:

- **SWD pads** (PA13/PA14) for debugging and last-resort recovery.
- **Hardware UART pads** (USART2, PA2/PA3). USB serial only exists once the
  firmware runs and USB is up: boot logs and crashes need the UART.

## Disk mode

With MSC, the computer owns the card's blocks while it's mounted. The player
can't touch the filesystem at the same time, or the card gets corrupted. So,
like the real iPod:

1. On plug-in: stop playback, unmount SdFat, show "Do not disconnect".
2. Hand the raw card to the computer. The syncer runs on the computer meanwhile.
3. On eject (or VBUS drop): remount, then reload or rebuild the index.

MTP would avoid the lockout (the device keeps owning the filesystem), but it's
slower, more complex, and macOS has no native support. MSC + disk mode wins.

The stock STM32duino USB stack only does CDC (or HID), not MSC: the firmware
needs TinyUSB on top of the HAL (it has a driver for the STM32 OTG
peripheral, dwc2, and does MSC + CDC composite).

## The options

### A. F446, USB Full Speed (current plan)

- **Speed:** 12 Mbit/s bus, **~1 MB/s** in practice for MSC.
- **SD card:** SPI is fine (~2 MB/s cap, already above what USB can move).
- **Pins:** none new. PA11/PA12 (D-/D+) and PA9 (VBUS sense) are already
  reserved in [`lqfp100-pins.md`](../datasheets/lqfp100-pins.md). Needs a real
  HSE crystal on the final board (the Nucleo borrows the ST-LINK's 8 MHz MCO).
- **Board:** USB 2.0-only 16-pin USB-C receptacle, 5.1 kΩ on CC1/CC2,
  USBLC6-2SC6 on D+/D-, VBUS divider. FS routing is forgiving: test points and
  jumpers on D+/D- are fine, which suits the debug-friendly carrier.
- **Firmware:** no port, same chip as today.
- **Debug:** same tools as today (ST-LINK, SWD, UART).

What 1 MB/s means:

| Transfer | Time |
|---|---|
| MP3 album (~100 MB) | ~2 min |
| FLAC album (~400 MB) | ~7 min |
| Full 64 GB card | ~18 h |

The syncer is additive, so day-to-day syncs stay at the "a few minutes" level.
The first bulk load goes through a card reader, if the SD card stays reachable
in the case.

### B. F446 + external ULPI PHY (USB High Speed)

The F446's OTG_HS only reaches 480 Mbit/s through an external ULPI PHY
(USB3300/USB3320 class, QFN, its own 24 MHz clock). Its 12 pins collide with
most of the LQFP100 allocation:

| ULPI signal | Pin | Currently |
|---|---|---|
| CK | PA5 | SD SCK |
| D0 | PA3 | Log UART RX |
| D1 | PB0 | BT CTS (optional) |
| D2 | PB1 | spare |
| D3 | PB10 | Backlight PWM |
| D4 | PB2 | PSRAM QSPI CLK (the only other QSPI CLK pin is PD3, in the TFT port) |
| D5 | PB12 | VS1053 XCS |
| D6 | PB13 | VS1053 SCK |
| D7 | PB5 | spare |
| STP | PC0 | SD_EN |
| DIR | PC2 | VS1053 XDCS |
| NXT | PC3 | VS1053 XRESET |

On top of that, HS only pays off with a fast SD path, and SPI isn't one. The
F446's 4-bit SDIO has its CMD line only on PD2, which is TFT D2.

- **Speed:** ~8-10 MB/s at best (SDIO and 128 KB of RAM become the limit).
- **Pins:** full re-pinout (display port, PSRAM, VS1053, SD), plus an extra chip.
- **Board:** a 90 Ω differential pair to the connector and a 60 MHz ULPI bus
  to the PHY, both to route carefully.
- **Firmware:** same chip, but new SD driver (SDIO) and display port.
- **Debug:** one more chip to bring up, and HS signal problems are hard to
  probe without a fast scope.

Not worth it: switching MCU is less work for more speed.

### C. STM32U5A5 (USB High Speed, on-chip PHY)

- **Speed:** 480 Mbit/s with the PHY built in, **~20-35 MB/s** for MSC
  (SD-card bound).
- **SD card:** must move to SDMMC 4-bit: ~20 MB/s at 50 MHz / 3.3 V; more with
  UHS-I, which needs an external 1.8 V level translator.
- **Pins:** no external PHY. The ROM bootloader does DFU on the HS port
  (PA11/PA12). A new pinout for a new chip anyway.
- **Bonus:** UCPD (USB-C CC detection and Power Delivery, no 5.1 kΩ guessing
  of the source current) and BC1.2 charger detection. See
  [`recaps/NUCLEO-U5A5ZJ-Q.md`](recaps/NUCLEO-U5A5ZJ-Q.md).
- **Board:** 90 Ω differential pair, short, no stubs: **no test points or
  jumpers on D+/D-**. The HS PHY needs an HSE crystal at one of its supported
  frequencies (check RM0456).
- **Firmware:** port to a new chip. To check first: a working STM32duino
  variant for the U5A5, and the clock tree setup for the PHY.
- **Debug:** new chip to learn (clocks, SMPS, power modes); HS signal problems
  need a fast scope. The USB stack itself (TinyUSB, MSC + CDC, DFU) is the same
  as option A.

## Comparison

| | A. F446 FS | B. F446 + ULPI | C. U5A5 HS |
|---|---|---|---|
| MSC speed | ~1 MB/s | ~8-10 MB/s | ~20-35 MB/s |
| 64 GB fill | ~18 h | ~2 h | ~40 min |
| New pins | 0 | 12 + full re-pinout | new chip, new pinout |
| Extra chips | none | ULPI PHY + clock | none (level translator only for UHS-I) |
| SD interface | SPI (as today) | SDIO (needs PD2, clashes with TFT) | SDMMC |
| Firmware port | none | SD + display rework | full port |
| PCB routing | forgiving | HS pair + ULPI bus | HS pair |
| Probing D+/D- | test points OK | no | no |
| Risk | low | high | medium |

## Recommendation

- **Carrier PCB: stay on the F446, ship USB Full Speed (A).** MSC, CDC and DFU
  all work, the pins are already reserved, and the routing stays probe-friendly.
- **Skip B.** It costs more than switching MCU and gets less.
- **Decide on C before the iPod-sized final board**, based on how annoying
  1 MB/s feels in real use, and on whether the SD card stays reachable for a
  card reader.
- The firmware work isn't wasted either way: TinyUSB MSC + CDC, disk mode and
  the DFU flow carry over to the U5A5. Only the clock setup and the SD driver
  change.

## Related: battery and protection

No MCU charges or protects a battery on its own. Those jobs go to dedicated
chips that keep working while the firmware is crashed, in the bootloader, or
unpowered:

- **Cell protection:** a protected LiPo cell (DW01A + FS8205 on the cell).
- **USB input:** USBLC6-2SC6 on D+/D-, overvoltage protection on VBUS (TVS or
  charger OVP), optional PTC fuse.
- **Charger:** a power-path one (BQ24075 or MCP73871) rather than the
  MCP73831, so the device boots from USB with a dead battery, for disk mode and
  DFU. The firmware picks the input current limit (100 mA before enumeration,
  500 mA after, more if the USB-C source advertises it on CC) through one or
  two pins.
- **Gauge:** MAX17048, already allocated (I2C1 0x36, ALRT on PE6).

The MCU only watches and sets limits: VBUS sense (PA9, reserved), charge status
(1 pin), current limit (1-2 pins), and optionally CC1/CC2 on two ADC pins on
the F446. That's 2-5 of the 16 spare pins.
