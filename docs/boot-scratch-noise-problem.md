# Boot scratch noise in the headphones

At startup the headphones make a scratch, like a jack being pulled out and plugged back in. Where does it come from, and what can fix it?

**TL;DR:** It happens whenever the VS1053's output switches off and back on. The firmware used to cause this itself on every boot, and that part is fixed. The rest happens while the Nucleo is held in reset or unpowered, when no firmware runs at all. Only hardware can fix that: a pull-up on the VS1053's XRESET line, and a mute or anti-pop stage for power loss.

## 1. Symptoms

- A scratch at boot, like a headphone jack going out and back in.
- The same noise as soon as the Nucleo's reset button is released.
- Two noises during a PlatformIO upload: one when the board reboots into upload mode, one when it reboots into the new firmware.
- The same noise when the Nucleo's USB cable is unplugged.

So the noise follows the MCU's resets and power, not anything the UI or the player does.

## 2. Cause: the VS1053 analog output collapsing

A hardware reset of the VS1053 (XRESET low) powers its analog drivers down. When the reset is released they power back up and the outputs settle to their bias level again. In the headphones that is one thump down and one thump up, which sounds just like the jack going out and back in.

There are three ways this can happen:

| When | Why XRESET goes low | Can firmware fix it? |
|---|---|---|
| Every boot (before the fix) | The driver pulsed XRESET itself, twice (see 3) | Yes, done |
| Reset button held, uploads | While the STM32 is in reset, all its pins float, including PA10 (`MP3_RESET`). With no pull-up, XRESET drifts low and the chip resets | No: no code runs during a reset |
| USB unplugged | If the VS1053 is powered from the Nucleo, it loses power outright | No |

## 3. What the firmware did, and what changed

Before the fix, `VS1053_driver::begin()` (`software/src/sound/VS1053_driver.cpp`) reset the chip twice on every boot, even though the chip had already reset itself at power-up:

1. `pinMode(_rst, OUTPUT)`: the STM32 output latch starts low, so just making the pin an output pulled XRESET low.
2. `reset()`: an explicit 50 ms low pulse on XRESET.

On top of that, PA10 floated from power-on until `player.setup()`, a few hundred milliseconds later (serial, display init and its delays), and could dip low on its own.

Now:

- **The reset pin is never pulsed by accident.** `begin()` writes XRESET high *before* `pinMode(_rst, OUTPUT)`.
- **A software reset is used when the chip answers.** `begin()` reads `SCI_STATUS` at the slowest SCI clock. If the chip responds (`SS_VER` = 4, checked by `VS1053_driver::isVS1053()`), it gets a software reset (`SM_RESET`), which keeps the analog outputs up. The hardware reset is now only a fallback when the chip doesn't answer. Since a floating MISO reads `0xFFFF` (`SS_VER` = 15), a missing chip no longer passes the check.
- **The output stays quiet during init.** Right after the reset, `SCI_VOL` is set to `0xFEFE`: maximum attenuation, analog drivers still powered. `0xFFFF` would power them down, and powering back up pops too. `PodPlayer::setup()` then sets the real volume. The old leftover `setVolume(60, 60)` is gone.
- **XRESET is held high from the first line of `setup()`** (`software/src/main.cpp`): `digitalWrite(MP3_RESET, HIGH); pinMode(MP3_RESET, OUTPUT);`. It is no longer left floating during boot.
- **The 1 s sine test tone in `PodPlayer::setup()` is gone.** The startup sound now does its job of confirming the audio chip works.

This removes the noises the firmware caused itself. It can't do anything about the time the MCU spends in reset or without power. Holding XRESET high on the first line of `setup()` only shortens the gap after the reset is released.

## 4. Hardware fixes

### Prototype (Nucleo + VS1053 breakout)

- **10 kΩ pull-up from XRESET to 3.3 V.** XRESET then stays high while the STM32 pins float, so the VS1053 keeps running through a reset button press or an upload. This is the fix for the reset and upload noise.
- **10 kΩ pull-ups on XCS and XDCS.** They keep the chip deselected while the SPI lines float, so noise on the bus can't turn into stray commands or data.
- If the breakout already has a pull-up on RST, it is too weak or not on the line we drive. Check it with a meter.

### Power loss (USB unplugged, battery switched off)

When the VS1053 loses power, its output collapses, and only hardware can hide that:

- a mute switch (e.g. a MOSFET) on the headphone outputs, closed slightly before the supply drops and opened after it is stable, or
- a headphone amplifier with built-in pop and click suppression.

This is a decision for the PCB.

## 5. PCB schematic review (`hardware/openpod/audio.kicad_sch`)

This comes from the netlist exported with `kicad-cli sch export netlist`. Some of it may just be work in progress, but each item affects this problem or the audio chip in general:

- **XRESET shares a net with the crystal.** The `VSRESET` net joins `VS1.3 (XRESET)` with `VS1.18 (XTALI)`, the 12.288 MHz crystal, C36 and R18. That looks like a label or wire placed on the wrong net. As drawn, the chip couldn't run properly.
- **The reset pull-up is dangling.** The `RESET_PULL` label only reaches `R17.2`. R17 is presumably the XRESET pull-up from section 4, but nothing joins it to XRESET.
- **The chip selects aren't connected.** `VS1.23 (XCS)` and `VS1.13 (XDCS/BSYNC)` are listed as unconnected, and they need their pull-ups too.
- **DREQ and reset don't reach the MCU.** The `VDREQ` and `VSRESET` global labels appear only in the audio sheet, so neither connects to the MCU or the Nucleo header.

What the board needs: XRESET on its own net, with R17 as a pull-up to 3.3 V and the line routed to `MP3_RESET`; XCS and XDCS routed to the MCU, each with a pull-up; DREQ routed to the MCU; and a mute or anti-pop stage on the headphone output.

## 6. How to check

1. Flash the current firmware and power-cycle the board. Without a pull-up, a noise at power-on or on releasing reset is still expected. A noise *later* in the boot (when the logo appears, or when the startup sound starts) would point back to the firmware.
2. Add the 10 kΩ XRESET pull-up. Pressing and releasing reset, and uploading, should then be silent.
3. Unplugging USB will still pop until there is a mute or anti-pop stage.
