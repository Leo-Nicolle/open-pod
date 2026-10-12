# Measuring each module's current drain

Goal: find out which modules need a load switch on the final board, and which
can be put to sleep by firmware instead. Each switch costs board space (a
TPS22976 dual is 3 x 2 mm, a TPS22917 is a SOT-23-6), so only rails that drain
too much while "off" get one.

Method: desolder the GND wire of one module at a time, put a multimeter in
series between the module's GND and the board's GND, and step the module
through its power states.

## Before starting

### Watch out for "phantom" ground paths

With the module's GND going through the meter, its ground sits slightly above
the board's ground (the meter's burden voltage). If that gets close to 0.5 V,
current starts flowing through the ESD diodes of the module's signal pins into
any MCU line driven low, and the meter under-reads. To keep this in check:

- Use the lowest range that doesn't overflow, and check the meter's burden
  voltage in its manual: on the µA range, multimeters typically drop between
  100 µV and 1 mV per µA, and a few hundred mV at full scale on the mA range.
- During each reading, measure the voltage between the module's GND and the
  board's GND with a second meter. Above about 100 mV, the reading isn't
  trustworthy: change range, or measure in the supply wire instead.
- Sanity check: the module's current, measured in its GND, should be about the
  same as the drop in the board's total current when you disconnect the module
  completely.

### Breakout boards aren't the final circuit

Most modules are breakouts with their own LDO, level shifters, pull-ups and
sometimes a power LED. The final board uses the bare chips, so:

- Note every extra part on each breakout (LDO, LED, level shifter).
- If a breakout has an LDO, measure once on its input as wired today, then,
  if you can, feed 3.3 V straight to the chip side to get the chip's own
  figure.
- Remove or cut power LEDs before measuring, or measure them separately.

### Equipment

- A multimeter with a µA range (and a second one for the burden check).
- Optional: a power profiler (Nordic PPK2, µCurrent, Joulescope) for loads
  that pulse (SD writes, BT streaming, MPR121 scanning). A multimeter only
  shows an unreliable average for these.

### Firmware

Write a test sketch (a new `software/src/main_power_test.cpp` env, like
`main_bt_test.cpp`) that moves each module through the states below on a
key press over the serial log, and prints the state it's in. All the MCU lines
to a module that is "off" should be driven low, or set as analog/hi-Z inputs,
so nothing back-powers it.

## What to measure

Note each state's current in the results table. "Rail off" is the floor: GND
fully disconnected, nothing measured (it's the reference for "needs a switch").

### SD card (SPI1, CS on PA4)

1. Card inserted, never initialised (right after power-up, CS high).
2. Initialised, idle (CS high, no transfer).
3. Continuous reading (play a track from the card).
4. Writing (copy a file), with a profiler if possible: write peaks can reach
   tens of mA.

There is no sleep command for SD cards in SPI mode: state 2 is the best
firmware can do. Try two or three cards from different brands, idle current
varies a lot between them.

### Screen (ILI9341)

Measure the backlight and the logic separately (the backlight already has its
own MOSFET on PB10).

1. Display on, showing the UI, backlight on.
2. Same, backlight off: the logic's current alone.
3. Display off (command `0x28`), backlight off.
4. Sleep In (command `0x10`), backlight off. Wait at least 5 ms after the
   command before reading.

If state 4 is low, the screen needs no load switch.

### VS1053 (SPI1, XCS on PA11, XRESET on PA10)

1. Playing an MP3.
2. Running but idle (no data sent, stream ended).
3. Analog power-down (`SCI_VOL = 0xFFFF`).
4. XRESET held low: the datasheet calls it "a full-powerdown mode" with all
   clocks stopped, but gives no current figure.

If state 4 is low, XRESET is the VS1053's power switch and VS_EN goes away.

### Bluetooth (FSC-BT1035, USART1, SYS_CTRL on PA15)

1. Powered, not booted (SYS_CTRL low).
2. Booted, not connected (discoverable / advertising).
3. Connected to a receiver, idle (no audio).
4. Streaming A2DP.
5. After a power-down AT command, if the firmware has one (check the
   programming guide).

The module has no true off state, so it is expected to need a switch: this
mainly confirms it, and gives the streaming current for the battery budget.
Measure VBAT_IN with and without its decoupling (still an open question from
the BT test).

### MPR121 (I2C3, IRQ on PH1)

1. Run mode at the firmware's current settings (touch wheel working).
2. Run mode with a longer sampling period (ESI / SFI in register 0x5D), to see
   how much it can be cut.
3. Stop mode (ECR = 0x00).

### DRV2605L (I2C3)

1. Idle after init.
2. During a vibration (with the motor connected; mostly for the budget).
3. Standby (MODE register, STANDBY bit set).
4. EN pin low, if the breakout exposes it (some tie EN high).

### PSRAM (APS6404L, SPI2, CS on PD2)

1. Idle, CS high.
2. During a read/write loop.
3. Its low-power mode, if the datasheet has one (check
   `datasheets/APS6404L-3SQR-SN-2(ram).pdf`).

### MCU, for reference

On the Nucleo-64, the IDD jumper (JP6) puts a meter in the MCU's supply only.
Measure run (80/180 MHz), Sleep, Stop and Standby. Remember that the ST-LINK
side of the Nucleo is powered separately and isn't counted.

## Results

Fill in one row per state, in µA (or mA when marked).

| Module | State | Current | Burden check (mV) | Notes |
|---|---|---|---|---|
| SD card | never initialised | | | brand: |
| SD card | idle after init | | | |
| SD card | reading | | | |
| SD card | writing (peak / avg) | | | |
| Screen | on + backlight | | | |
| Screen | on, backlight off | | | |
| Screen | display off | | | |
| Screen | Sleep In | | | |
| VS1053 | playing | | | |
| VS1053 | idle | | | |
| VS1053 | analog power-down | | | |
| VS1053 | XRESET low | | | |
| BT | not booted | | | |
| BT | booted, not connected | | | |
| BT | connected, idle | | | |
| BT | streaming | | | |
| MPR121 | run (current settings) | | | |
| MPR121 | run (slower sampling) | | | |
| MPR121 | stop | | | |
| DRV2605L | idle | | | |
| DRV2605L | standby | | | |
| DRV2605L | EN low | | | |
| PSRAM | idle | | | |
| PSRAM | low-power mode | | | |
| MCU | Stop | | | |
| MCU | Standby | | | |

## Deciding

For each module, take its lowest firmware state (sleep, reset, stop) and
compare it to the cost of a switch:

- **A few µA or less:** no switch, firmware puts it to sleep. A switch would
  only save that much, and a TPS22976 channel itself leaks about 1.4 µA when
  off.
- **Tens of µA or more:** it gets a switch. Group these on TPS22976 dual
  channels, pairing modules that can share one (SD and VS1053 are often used
  together, BT is independent).

To set the real threshold, add up the standby currents and divide the battery
capacity by the total. For example, with a 600 mAh battery, 100 µA of total
standby current empties it in about 250 days; 1 mA takes about 25 days.

Then update `datasheets/lqfp100-pins.md` and the LQFP100 mapping: every module
that doesn't need a switch frees its `_EN` pin.
