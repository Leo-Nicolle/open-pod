# Next step: bigger Nucleo, Bluetooth validation, carrier PCB

Where the project stands, what's left to validate, and the plan to get there.

## Where we are

- **Validated on the bench:** screen, VS1053, control buttons, SD card, haptic
  (DRV2605L), touch wheel (MPR121) and Bluetooth (FSC-BT1035).
- **Bluetooth: done.** The module, wired by hand to the current Nucleo (see
  [`BT-pinout.md`](BT-pinout.md)), takes AT commands over USART1, pairs with a
  receiver and streams the VS1053's I2S output (A2DP source, SBC). The player
  drives it (`software/src/bluetooth/Bluetooth.hpp`), and MP3/FLAC track changes don't
  glitch. Bring-up sketch: `software/src/main_bt_test.cpp`.
- **Firmware:** does almost everything. Missing: power management, battery
  management, and USB file transfer to the SD card.
- **Pins:** the Nucleo-64's STM32F446RE (LQFP64, 50 GPIOs) is nearly full
  (see [`next-step-pinout.md`](next-step-pinout.md)).
- **Dev board:** a hand-soldered perfboard. It works, but it's hard to inspect
  or probe.
- **BT devboard:** the first one was wrong (footprint and schematic). Both have
  been redone: side-mounted cutout footprint with the corrected pin order, and a
  new schematic with a TPS22917 power switch, the datasheet's auto power-on
  circuit and its line-in network.

Still to validate: power buttons, power management, battery and USB.

## Recommendation

**Bluetooth is validated. Buy a NUCLEO-F446ZE, then design an "in-between"
carrier PCB. Don't go iPod-sized yet.**

### 1. Validate Bluetooth on the current Nucleo: done

The BT path costs almost no MCU pins:

- Control: the UART is already on USART1 (PB6/PB7), plus one enable pin
  (PA15).
- Audio: the VS1053 I2S output goes straight to the module. It doesn't go
  through the STM32.

Validated with the module wired by hand, without a devboard: the Feasycom
firmware does A2DP source from I2S input (`AT+AUXCFG=3`, `AT+I2SCFG=3`, with
the VS1053 as I2S master at 48 kHz, 16 bit). The BT module and the audio path
stay as designed.

Still worth checking: what happens when the receiver drops and comes back, and
whether VBAT_IN needs its decoupling (left off on the wired test). The BT
devboard PCB (`hardware/openpod/bt_devboard.kicad_sch`) is no longer blocking:
the carrier PCB can carry the module directly.

### 2. Buy a NUCLEO-F446ZE, stop squeezing the 64-pin one

Same chip (F446) in LQFP144, so the firmware carries over. It's cheap compared
to the time spent juggling pins.

- It removes the pin and bus problems: one SPI bus per fast device, and a full
  port for the screen (one register write per pixel).
- Use only pins that also exist on the LQFP100 package, following
  `datasheets/lqfp100-pins.md`. That way you
  validate the final board's pinout before it goes to copper.
- Room for what's still missing: power management, battery and USB need pins
  that don't exist today.

Watch out for the board's own wiring (to check in UM1974): the ST-LINK virtual
COM port on PD8/PD9 and the user LEDs on PB0/PB7/PB14 clash with the LQFP100
allocation. Details in `lqfp100-pins.md`.

### 3. Next PCB: a carrier board, not iPod-sized

A board that plugs onto the Nucleo-144's morpho headers and carries the real
circuits:

- **Power:** Li-ion charger, TPS22917 switches for the SD card, the VS1053 and
  BT, MAX17048 fuel gauge, power button and lock switch.
- **USB:** connector wired to USB FS (PA11/PA12) plus VBUS sense, to test SD
  transfers and charger detection.
- **Peripherals:** SD socket, VS1053 (chip, or its breakout on headers), the BT
  module cutout, connectors for the screen FPC and the click wheel, and the
  haptic and touch-wheel chips or breakouts.
- **Built for debugging:**
  - test points on every signal;
  - 0R links or jumpers to cut each rail, to measure each subsystem's current
    (needed for the battery-life budget);
  - LEDs on the rails;
  - generous spacing.

It replaces the perfboard with something you can probe. It also validates the
power and USB circuits, which breakouts can't really test.

**Why not iPod-sized yet:** power and USB are still unvalidated. A tight layout
turns every mistake into a full respin that's hard to debug. Use the carrier to
measure real currents, then do the iPod-sized board once all circuits are
proven. It's still worth checking now that the key parts fit the case outline,
just don't constrain the carrier to it.

## Package for the final board

| Package | GPIOs | Verdict |
|---|---|---|
| LQFP64 (current) | 50 | Too few. |
| WLCSP81 | 63 | Fewer pins than LQFP100. 0.4 mm-pitch balls need an HDI board and machine assembly. |
| **LQFP100** | **81** | **Recommended.** 14 x 14 mm fits easily in an iPod-classic case, hand-solderable and inspectable. The allocation in `lqfp100-pins.md` leaves 16 pins spare. |
| UFBGA144 | 114 | 10 x 10 mm, but needs a 4+ layer board, machine assembly and X-ray inspection. The extra pins aren't needed. |

GPIO counts are from the F446 datasheet (`datasheets/stm32f446re.pdf`). The
full LQFP100 pin table, extracted from that datasheet, is in
`docs/public/pinout/LQFP100/pins.json`: pick the LQFP100 package in the
[pinout studio](../docs/pinout.md) to see it with the allocation.

## Order of work

1. ~~Lay out, order and assemble the BT devboard.~~ Skipped: wired by hand.
2. ~~On the current Nucleo: AT commands over USART1, pairing, then VS1053 I2S
   to BT (A2DP source).~~ Done.
3. NUCLEO-F446ZE: port the firmware to the LQFP100 pinout (display on port D,
   PSRAM on QSPI, VS1053 on SPI2, SD alone on SPI1, I2C1, BT on UART4). The
   list of firmware changes is in `lqfp100-pins.md`.
4. Carrier PCB: power, battery, USB, real connectors and test points.
5. Power and current measurements on the carrier, then firmware for power
   management, battery and USB transfers.
6. iPod-sized final board with the LQFP100.
