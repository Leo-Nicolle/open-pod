# Next step: pinout for battery, lock and power management

Plan for fitting the lock switch, the power MOSFETs (backlight, SD card,
Bluetooth) and battery monitoring into the STM32F446RE's pins. Starting point:
[`docs/public/pinout/LQFP64/mapping.json`](../docs/public/pinout/LQFP64/mapping.json) and `software/src/pinout.h`.

**Verdict: it fits.** On the current Nucleo it fits with no spare pins. Once
PC14/PC15 are reconnected (see below) there are 2 slow pins of margin.

## Where we are now

The F446RE in LQFP64 has 51 GPIOs. PB11 does not exist on this package (that
pin is VCAP1). With `BROKEN_NUCLEO`, the remaining pins are all spoken for:

| Pin | Status |
|---|---|
| PA2/PA3 | USART2 to the ST-Link, used for `Serial` logs. Keep for now. |
| PA13/PA14 | SWD. Keep. |
| PH0 | Probably not free: on Nucleo-64 boards it is normally fed by the ST-Link's 8 MHz clock output through a solder bridge. Check the board. |
| PC14/PC15 | "Dead" (see below). |

So there are **0 spare pins** today.

## Pins we can free

1. **PB10 `TFT_RD`: free it.** The display is only ever read by the ID and
   diagnostics helpers (`readRegisterBytes`/`read16` in
   `software/src/rendering/ILI9341_driver.h`). Tie RD to 3V3 on the board.
   PB10 is TIM2_CH3, so it can do PWM, which makes it the natural backlight
   pin.
2. **PB12 `TFT_RST`: free it.** Tie the display's RESET to the STM32's NRST,
   or to an RC circuit. The `0x01` software reset command still works if we
   want one in the init.
3. **PA15 `BT_RESET`: reuse it.** It is defined in `pinout.h` but never used in
   the code. Put the BT module's power MOSFET on this pin instead:
   power-cycling the module is a reset anyway.
4. **PA10 `MP3_RESET`: free it, with one condition.** The code only ever sets
   it HIGH (`main.cpp`). However, holding XRESET low is the VS1053's only real
   low-power state. We can only drop this pin if the VS1053 goes on a switched
   supply. Sharing the SD card's switch works: both chips sit on SPI1 and are
   both idle when nothing is playing.
5. **PC14/PC15 are not dead, just disconnected.** Confirmed in the Nucleo
   user manual (UM1724, `datasheets/nucleo.pdf`, "OSC 32 kHz clock supply" and
   table 10 "Solder bridges"): on MB1136 C-02 and later, PC14/PC15 go to the
   X2 32 kHz crystal and SB48/SB49 (the link to the CN7 header) ship open.
   That's why continuity from the FPC to the header was fine but the pins
   never moved. To get them back as GPIO:
   - **Close SB48 and SB49** with a solder blob. No resistor: a solder bridge
     is just two bare pads.
   - **Remove R34 and R36.** These connect PC14/PC15 to the crystal and its
     load capacitors. Leave them in and the crystal stays hanging on the line.

   That gives back 2 pins. See the next section for what to put on them.

### Which pins should carry the data bus?

PC13, PC14 and PC15 sit in the backup power domain. The F446 datasheet limits
them to 2 MHz (30 pF load), 3 mA sink, and no sourcing.

**The math.** A 320x240 frame is 76,800 pixels, one 16-bit word (one WR
pulse) each. A data line toggles fastest when its bit flips on every pixel, so
its frequency is half the word rate:

| Line toggle | Word rate | Full frames/s |
|---|---|---|
| 2 MHz (pin limit) | 4 Mpx/s | 52 fps |

**Measured** with `software/src/main_display_bench.cpp`
(`pio run -e display_bench -t upload`), 180 MHz core, `BROKEN_NUCLEO` build:

| Path | Cycles/px | Rate | Full frames/s | Data line toggle (worst case) |
|---|---|---|---|---|
| `fillScreen` | 12.8 | 14.0 Mpx/s | 183 fps | 0: the color is constant, only WR toggles |
| `pushPixels` (UI text/images) | 38.3 | 4.7 Mpx/s | 61 fps | 2.35 MHz |

So the only path that moves the data lines is `pushPixels`, and only an
every-other-pixel pattern on that bit reaches 2.35 MHz, about 18% over the
limit. Real content toggles much less. PC13-PC15 are marginal as data lines,
not hopeless: D13 has worked on PC13 since the start.

(Side note: `fillScreen` at 14 Mpx/s is a 71 ns write cycle, right at the
ILI9341's 66 ns minimum. That's the path to slow down first if fills ever
glitch.)

- **D14/D15 stay on PA0/PA1** (the `BROKEN_NUCLEO` layout), even after the
  solder-bridge fix. Not because PC14/PC15 couldn't carry them, but because we
  need homes for slow signals anyway (lock switch, MOSFET enables), and those
  pins suit slow signals perfectly. Putting the data there instead would just
  push the slow signals onto PA0/PA1.
- **D13 is still on PC13**, marginal in the same way. It works on the bench
  today, so leave it on the Nucleo. On the Nucleo the B1 user button
  also hangs on PC13 (SB17 ON); open SB17 if D13 ever looks flaky.
- **On the final PCB, move D13 off PC13 too**, onto a normal pin driven
  through BSRR like D14/D15 (e.g. one of the pins freed above). PC13 then
  becomes a third slow GPIO. The pin count doesn't change; you just swap which
  pin does what.

### Firmware: PC14/PC15 are already masked out

Under `BROKEN_NUCLEO`, the display driver no longer touches PC14/PC15
(`BUS_PORTC_MASK` in `software/src/rendering/ILI9341_driver.h`):

- Mode, type and speed setup only touch PC0-PC13.
- Bus writes go through `portCReg()`/`portCWord()`, which is a `GPIOC->BSRR`
  store that sets or resets PC0-PC13 only. Before, a `GPIOC->ODR = data`
  write on every pixel would have overwritten whatever PC14/PC15 were driving.
- The fast loops in `ILI9341_GFX.h` (`fillPixels`, `pushPixelSlice`) use the
  same helpers.

Without `BROKEN_NUCLEO`, the bus is unchanged: a plain `GPIOC->ODR` write over
all 16 pins.

## Proposed allocation

| Function | Pin | Notes |
|---|---|---|
| Backlight PWM | PB10 | TIM2_CH3, low-side N-MOSFET. |
| SD (+ VS1053) power | PB12 | Use a load switch with slew control (e.g. TPS22917), not a bare P-FET, or the inrush can brown out the 3V3 rail. |
| BT power | PA15 | Replaces `BT_RESET`. |
| Lock switch | PA10 | Or PC14/PC15 once the solder bridges are fixed. |
| Battery level | **0 pins** | MAX17048 fuel gauge on I2C3 at 0x36. No conflict with the DRV2605L (0x5A) or the MPR121 (0x5B), and more accurate than a voltage divider on a Li-ion's flat discharge curve. |
| Spare | PC14, PC15 | Only after the solder-bridge fix. Slow signals only (see above). |

## The lock button

Physically cutting the buttons' common wire only disables the 5 mechanical
buttons. The MPR121 touch wheel keeps working, and the UI can't show a lock
icon. So the switch needs a sense input either way.

Zero-pin option: the MPR121's unused electrodes ELE4-11 can work as GPIOs if
the wheel uses ELE0-3. The lock would then be polled over I2C.

## Safety valve if we run out later

Add a TCA9534/PCA9554 I/O expander on I2C3 and wire-OR its INT line with the
MPR121 IRQ on PH1. That gives 8 I/Os for 0 extra MCU pins. The 5 buttons could
move there, which frees PB0/1/2/5 and PA12.

## Gotchas with power-gating

- **Back-powering:** an unpowered SD card or BT module gets powered through
  its I/O pins' protection diodes.
  - SPI1 keeps toggling for the VS1053, and CARDCS is held high. Drive CARDCS
    low when the SD card is off.
  - For BT, set PB6 to input and turn off the VS1053's I2S output before
    cutting power.
- **Re-init after power-up:** the SD card (SdFat) and the BT module both need
  re-initialising every time they are switched back on.

## Follow-ups

- Update `docs/public/pinout/LQFP64/mapping.json` and `software/src/pinout.h` once the allocation
  is final.
- In `ILI9341_driver.h`, drop the `TFT_RD`/`TFT_RST` pin handling (and the ID
  read diagnostics) once those lines are tied on the board.
- After closing SB48/SB49, update the `BROKEN_NUCLEO` notes in
  `platformio.ini` and `.agents/screen-red-problem.md`: the cause is now
  known.
- Final PCB: move D13 off PC13 (see "Which pins should carry the data bus?").
