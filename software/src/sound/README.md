# Enhanced Audio Flow for VS1053b

This directory contains the enhanced audio flow implementation for the VS1053b audio decoder chip, optimized for high-quality audio playback based on comprehensive datasheet analysis.

## Key Improvements

### 1. Optimal Clock Configuration

The VS1053b driver now implements proper clock configuration based on the official datasheet:

- **Default Configuration**: 3.0x clock multiplier with 12.288MHz crystal
- **Internal Clock**: 36.864MHz (12.288MHz × 3.0)
- **Maximum Sample Rate**: 144kHz (36.864MHz ÷ 256)
- **48kHz Support**: Fully supported with significant headroom
- **SM_CLK_RANGE Support**: Automatic configuration for 24-26MHz crystals

### 2. Enhanced SPI Communication

- **Datasheet-Based SPI Speed**: 7MHz base × clock multiplier, with 80% safety margin
- **Optimal Burst Transmission**: 32-byte bursts per VS1053b specifications
- **DREQ Respect**: Proper DREQ checking every 32 bytes as recommended
- **Improved Throughput**: Better utilization of VS1053b's 2048-byte internal FIFO

### 3. Intelligent Buffer Management

- **32-Byte Burst Feeding**: Matches VS1053b optimal burst size
- **Enhanced Priming**: Optimized initial buffer feeds
- **FIFO-Aware Feeding**: Respects 2048-byte FIFO size (not 8KB as previously assumed)
- **PSRAM Integration**: Seamless preloading for gapless playback

### 4. Advanced Features

- **High-Frequency Crystal Support**: Automatic SM_CLK_RANGE for 24-26MHz crystals
- **Streaming Mode Ready**: Foundation for webradio-style streaming
- **PCM Streaming Support**: Ready for continuous PCM audio
- **Dynamic Clock Scaling**: SC_ADD support for WMA/AAC automatic scaling

### 5. Datasheet Compliance

All improvements follow the VS1053b datasheet specifications:

- Proper DREQ waiting after clock configuration
- Correct handling of temporary 1.0x clock during changes
- Support for 12-13MHz and 24-26MHz crystal ranges
- Optimal SC_MULT and SC_ADD register configuration
- 32-byte burst transmission with DREQ respect

## Clock Multiplier Settings

| SC_MULT | Value  | Multiplier | Internal Clock (12.288MHz) | Max Sample Rate |
| ------- | ------ | ---------- | -------------------------- | --------------- |
| 0       | 0x0000 | 1.0x       | 12.288MHz                  | 48kHz           |
| 1       | 0x2000 | 2.0x       | 24.576MHz                  | 96kHz           |
| 2       | 0x4000 | 2.5x       | 30.720MHz                  | 120kHz          |
| 3       | 0x6000 | 3.0x       | 36.864MHz                  | 144kHz          |
| 4       | 0x8000 | 3.5x       | 43.008MHz                  | 168kHz          |
| 5       | 0xa000 | 4.0x       | 49.152MHz                  | 192kHz          |
| 6       | 0xc000 | 4.5x       | 55.296MHz                  | 216kHz          |
| 7       | 0xe000 | 5.0x       | 61.440MHz                  | 240kHz          |

## Usage Examples

### Basic Initialization

```cpp
PodPlayer player;
player.setup(); // Automatically configures optimal clock settings
```

### Custom Audio Quality Configuration

```cpp
// Configure for specific sample rate
player.audioPlayer.configureAudioQuality(48000); // 48kHz target

// Check maximum supported sample rate
uint32_t maxRate = player.audioPlayer.getMaxSampleRate();
Serial.printf("Max sample rate: %lu Hz\n", maxRate);
```

### Advanced Clock Configuration

```cpp
// Access VS1053 driver directly for custom settings
VS1053_driver& driver = player.audioPlayer.getDriver();

// Configure 4.0x multiplier for maximum performance
driver.setClockMultiplier(VS1053_SC_MULT_4_0X, VS1053_SC_ADD_1_0X);

// Set custom crystal frequency
driver.setClockFrequency(24576000); // 24.576MHz crystal
```

## File Structure

- **VS1053_driver.h/cpp**: Core VS1053 communication with enhanced clock configuration
- **AudioPlayer.h/cpp**: High-level audio player with quality configuration methods
- **Audio_buffer.h/cpp**: Buffer management with PSRAM support
- **player.h/cpp**: Application-level player implementation

## Performance Benefits

1. **Higher Quality Audio**: Support for 48kHz+ sample rates
2. **Improved Stability**: Proper clock sequencing prevents audio glitches
3. **Better Throughput**: Optimized SPI speeds reduce buffer underruns
4. **Gapless Playback**: Enhanced buffer management with PSRAM preloading
5. **Future-Proof**: Support for higher crystal frequencies and sample rates

## Technical Notes

- The VS1053 internal FIFO is 8KB (2048 stereo samples)
- Clock changes may cause temporary 1.0x operation for ~200 cycles
- SPI speed is automatically optimized but capped at safe limits
- DREQ timing is critical and properly handled throughout
- All register operations include proper timeout handling
