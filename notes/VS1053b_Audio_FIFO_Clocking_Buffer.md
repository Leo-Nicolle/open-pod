
# VS1053b: Audio Transmission, FIFO, Clocking, and Buffer Details

## 📦 Audio FIFO and Buffering

- **FIFO Depth**: The VS1053b includes a 2048-byte (1 KiB) stereo FIFO buffer for input data. This allows smooth streaming and decoding of audio data.
- **Data Flow**:
  - Input data arrives through the SDI (Serial Data Interface) and is buffered into the FIFO.
  - Decoded samples are passed to a DAC FIFO.
- **DREQ Pin**:
  - DREQ indicates whether VS1053b is ready to receive more data.
  - When DREQ is high, the device can accept at least 32 bytes of SDI data or one SCI command.
  - When DREQ is low, the host must stop sending data until it rises again.
  - DREQ can also be read via `SCI_WRAMADDR` = `0xC012` and `SCI_WRAM`.
- **FIFO and SDI Transmission**:
  - You may burst-send up to 32 bytes of SDI without checking DREQ again.
  - It is recommended to send data in 32-byte chunks, and toggle `XDCS` every now and then to resynchronize.
- **FIFO Internal Use**:
  - Buffering ensures uninterrupted decoding even with some jitter in data arrival.

## ⏱ Input Clock Frequency and Internal Clock

- **XTALI (Input Clock)**: Nominally 12.288 MHz. Acceptable range is **12 MHz to 13 MHz**.
- **Internal Clock Multiplier (PLL)**:
  - Controlled via `SCI_CLOCKF` register.
  - Internal clock (CLKI) = XTALI × multiplier.
  - Supported multipliers range from **1.0× to 5.0×**.
  - Default is **1.0×**, must be increased for real-time decoding of high-bitrate content (e.g., AAC with PS/SBR).
- **SCI_CLOCKF Register Format**:
  - `SC_MULT[15:13]`: Clock multiplier (0 = ×1.0, 1 = ×2.0, …, 7 = ×5.0)
  - `SC_ADD[12:11]`: Addition for dynamic clock scaling (e.g., WMA)
  - `SC_FREQ[10:0]`: XTALI tuning: `(XTALI - 8000000) / 4000`.
  - Example: `SCI_CLOCKF = 0x8BE8` means: XTALI=12 MHz, multiplier=3.5× + 1.0× → 54 MHz internal clock.
- **Maximum Internal Clock (CLKI)**: 55.3 MHz
- **After setting SCI_CLOCKF**, wait for **DREQ to rise** before sending further data.

## 📶 SM_CLK_RANGE and Clock Dividers

- **SM_CLK_RANGE** is a bit in the `SCI_MODE` register.
- If set, allows use of 24–26 MHz crystals by dividing XTALI by 2.
- Use when using high-frequency crystals. For 24.576 MHz crystal: enable `SM_CLK_RANGE` to simulate 12.288 MHz operation.
- **Must be set immediately after hardware reset** if using 24–26 MHz.

## 🔃 SPI Speed and Dynamic Scaling

- SPI communication is influenced by internal clock (CLKI).
- System starts with CLKI = XTALI (1.0×). Higher SPI speeds are permitted **after** clock is increased via `SCI_CLOCKF`.
- **Maximum SPI input speed for SDI** is **7 MHz** at default clock, scales up with CLKI.
- **SCI speed** is limited by combined timing of `tWL + tWH + tH = 6 × CLKI + 25 ns` → SCI reads max speed ~CLKI / 7.

## 🎧 Audio Transmission and Playback

### Playback via SDI

- Data is transmitted via the SDI bus.
- Data should be streamed regularly and in small chunks.
- **DREQ should be respected every 32 bytes.**

### Streaming Mode

- Set `SM_STREAM` bit in `SCI_MODE` to activate.
- Ideal for webradio-style data streaming.
- VS1053 adjusts playback rate by ±5% to avoid buffer underflow.
- Requirements:
  - Bitrate ≤ 160 kbps
  - CBR preferred, no VBR
  - Sample rate ≤ 48 kHz
- Accuracy: Less than ±0.5% error yields optimal playback.

### PCM Streaming

- Send WAV header (e.g., with length 0xFFFFFFFF) to enter continuous PCM mode.
- Supports 8-bit or 16-bit, mono or stereo samples.
- Sample rate defined in WAV header.
- If header not sent, defaults to 44.1 kHz stereo.

## 🧮 DAC Clock and Resampling

- DAC operates at fixed internal clock (XTALI / 2), e.g., 6.144 MHz when XTALI=12.288 MHz.
- Final audio rate is interpolated to a **common sample rate** internally.
- DAC is oversampled **128×**, final resolution is **18 bits**.
- Zero-cross detection used for volume changes.

## 🔁 Timer Module and FIFO Synchronization

- VS1053 has 2× 32-bit timers for general timing purposes.
- Timers are driven from a clock divider of the master clock.
- Useful for precise control of recording/playback durations.
- Not typically needed for regular audio streaming.

## 📚 Related Registers Summary

| Register       | Function                          |
|----------------|-----------------------------------|
| SCI_MODE       | Mode flags (SM_STREAM, SM_CLK_RANGE) |
| SCI_CLOCKF     | Clock multiplier and input freq     |
| SCI_AUDATA     | Sample rate, stereo/mono setting    |
| SCI_WRAMADDR   | Memory read/write address           |
| SCI_WRAM       | Memory data for WRAMADDR            |
| DAC_FCTL       | DAC control (sample rate, PLL freq) |
| DREQ           | Data request pin (status feedback)  |

---

This document summarizes all audio transmission and timing details from the VS1053b datasheet.
