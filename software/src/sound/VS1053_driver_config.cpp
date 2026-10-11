/*!
 * @file VS1053_driver_config.cpp
 * VS1053 clock / crystal configuration and FLAC plugin loading (the rest of
 * the driver is in VS1053_driver.cpp)
 */

#include "VS1053_driver.h"
#include "VS1053_FLAC_plugin.h"
#include "../logging.h"

// OPTIMIZED: FLAC-specific clock configuration matching working implementation
bool VS1053_driver::configureFLACClock() {
  if (readRegister(VS1053_REG_CLOCKF) == VS1053_FLAC_SPEED) {
    return true; // already set by a previous FLAC start
  }
  LOG_CAT(VS1053, "Configuring VS1053 clock for FLAC playback");

  // SC_MULT 4.5x: 12.288 MHz * 4.5 = 55.3 MHz, the chip's maximum. FLAC
  // decoding is CPU-bound in the plugin, so it gets all of it.
  writeRegister(VS1053_REG_CLOCKF, VS1053_FLAC_SPEED);

  // Critical: Wait for DREQ after clock change
  waitForDREQ();
  delay(50);

  // Verify the clock configuration
  uint16_t clockf = readRegister(VS1053_REG_CLOCKF);
  if (clockf != VS1053_FLAC_SPEED) {
    LOG_ERROR("FLAC clock configuration failed: 0x%04X", clockf);
    return false;
  }

  // writeRegister() already re-derived the SPI clocks from the new CLOCKF.
  LOG_CAT(VS1053, "FLAC clock configured: SCI %lu Hz, SDI %lu Hz",
          (unsigned long)_sciFreq, (unsigned long)_sdiFreq);
  return true;
}

bool VS1053_driver::configureOptimalClock(uint32_t xtalFreq) {
  LOG_CAT(VS1053, "Configuring VS1053 clock for %lu Hz crystal", xtalFreq);

  // Start with 1.0x multiplier (chip default after reset)
  // This ensures we're in a known state
  setClockMultiplier(VS1053_SC_MULT_1_0X, VS1053_SC_ADD_NONE);

  // Wait for DREQ to ensure clock is stable
  waitForDREQ();
  delay(10);

  // Set the crystal frequency if different from 12.288MHz
  if (xtalFreq != VS1053_XTALI_12_288MHZ) {
    setClockFrequency(xtalFreq);
    waitForDREQ();
    delay(10);
  }

  // Configure conservative multiplier for reliable audio
  // 2.0x multiplier provides good balance of performance and stability
  // With 12.288MHz crystal: 2.0x = 24.576MHz internal clock
  // Max sample rate = 24.576MHz / 256 = 96kHz (sufficient for 48kHz)
  setClockMultiplier(VS1053_SC_MULT_2_0X, VS1053_SC_ADD_NONE);

  // Critical: Wait for DREQ after clock configuration change
  // The datasheet states the chip may run at 1.0x for a few hundred cycles
  waitForDREQ();
  delay(50); // Give extra time for clock stabilization

  // Verify the clock configuration took effect
  uint16_t clockf = readRegister(VS1053_REG_CLOCKF);
  LOG_CAT(VS1053, "Clock configured: 0x%04X", clockf);

  // Now we can optimize SPI speed since internal clock is higher
  optimizeSPISpeed();

  // Verify chip is still responsive
  uint16_t status = readRegister(VS1053_REG_STATUS);
  if (!(status & 0x0040)) {
    LOG_ERROR("VS1053 not responding after clock configuration");
    return false;
  }

  LOG_CAT(VS1053, "Max sample rate: %lu Hz", getMaxSampleRate());
  return true;
}

void VS1053_driver::setClockMultiplier(uint16_t scMult, uint16_t scAdd) {
  // Combine SC_MULT and SC_ADD values
  uint16_t clockf = scMult | scAdd;

  LOG_CAT(VS1053, "Setting clock multiplier: 0x%04X", clockf);

  // Write to CLOCKF register
  writeRegister(VS1053_REG_CLOCKF, clockf);
}

void VS1053_driver::setClockFrequency(uint32_t xtalFreq) {
  // Calculate SC_FREQ value: (XTALI - 8000000) / 4000
  // This tells the chip the actual crystal frequency
  if (xtalFreq < 8000000UL) {
    LOG_WARN("Crystal frequency too low");
    return;
  }

  uint16_t scFreq = (xtalFreq - 8000000UL) / 4000UL;

  // Read current CLOCKF value to preserve SC_MULT and SC_ADD
  uint16_t currentClockf = readRegister(VS1053_REG_CLOCKF);
  uint16_t scMult = currentClockf & 0xE000; // Preserve SC_MULT
  uint16_t scAdd = currentClockf & 0x1800;  // Preserve SC_ADD

  // Combine with new SC_FREQ
  uint16_t newClockf = scMult | scAdd | (scFreq & 0x07FF);

  LOG_CAT(VS1053, "Setting crystal frequency: %lu Hz (SC_FREQ: 0x%03X)",
          xtalFreq, scFreq);

  writeRegister(VS1053_REG_CLOCKF, newClockf);
}

uint32_t VS1053_driver::getMaxSampleRate() {
  uint16_t clockf = readRegister(VS1053_REG_CLOCKF);

  // Extract SC_FREQ (bits 10:0)
  uint16_t scFreq = clockf & 0x07FF;
  uint32_t xtalFreq = (scFreq * 4000UL) + 8000000UL;

  // Extract SC_MULT (bits 15:13)
  uint16_t scMult = (clockf & 0xE000) >> 13;

  // Calculate multiplier value
  float multiplier;
  switch (scMult) {
  case 0:
    multiplier = 1.0f;
    break;
  case 1:
    multiplier = 2.0f;
    break;
  case 2:
    multiplier = 2.5f;
    break;
  case 3:
    multiplier = 3.0f;
    break;
  case 4:
    multiplier = 3.5f;
    break;
  case 5:
    multiplier = 4.0f;
    break;
  case 6:
    multiplier = 4.5f;
    break;
  case 7:
    multiplier = 5.0f;
    break;
  default:
    multiplier = 1.0f;
    break;
  }

  // Internal clock = XTALI × multiplier
  uint32_t internalClock = (uint32_t)(xtalFreq * multiplier);

  // Max sample rate = Internal clock / 256
  return internalClock / 256UL;
}

void VS1053_driver::optimizeSPISpeed() {
  updateClockSpeeds(readRegister(VS1053_REG_CLOCKF));
  LOG_CAT(VS1053, "VS1053 SPI: SCI %lu Hz, SDI %lu Hz",
          (unsigned long)_sciFreq, (unsigned long)_sdiFreq);
}

void VS1053_driver::configureClockRange(uint32_t xtalFreq) {
  // Configure SM_CLK_RANGE for 24-26MHz crystals per datasheet
  // Must be set immediately after hardware reset if using high-frequency
  // crystals

  if (xtalFreq >= 24000000UL && xtalFreq <= 26000000UL) {
    LOG_CAT(VS1053, "Enabling SM_CLK_RANGE for %lu Hz crystal", xtalFreq);

    // Read current mode register
    uint16_t mode = readRegister(VS1053_REG_MODE);

    // Set SM_CLK_RANGE bit to divide XTALI by 2
    mode |= VS1053_MODE_SM_CLKRANGE;

    // Write back the mode register
    writeRegister(VS1053_REG_MODE, mode);

    // Wait for DREQ after mode change
    waitForDREQ();
    delay(10);

    LOG_CAT(VS1053, "SM_CLK_RANGE enabled - crystal frequency divided by 2");
  } else if (xtalFreq >= 12000000UL && xtalFreq <= 13000000UL) {
    LOG_CAT(VS1053, "Using standard clock range for %lu Hz crystal", xtalFreq);
  } else {
    LOG_WARN("Crystal frequency %lu Hz is outside recommended range",
        xtalFreq);
  }
}

// FLAC Plugin support - plugin data is now in VS1053_FLAC_plugin.h

bool VS1053_driver::loadFLACPlugin() {
  LOG_CAT(VS1053, "Loading FLAC plugin...");

  // Check if VS1053 is ready
  if (!readyForData()) {
    LOG_ERROR("VS1053 not ready for FLAC plugin");
    return false;
  }

  // Set clock for FLAC processing first - CRITICAL FOR FLAC PERFORMANCE
  LOG_CAT(VS1053, "Setting FLAC clock configuration...");
  if (!configureFLACClock()) {
    return false;
  }

  // Now load the actual FLAC plugin data
  LOG_CAT(VS1053, "Loading FLAC plugin data (%d words)...", FLAC_PLUGIN_SIZE);

  // Load plugin using the proper VS1053 plugin loading method
  // This follows the VLSI Solution plugin loading methodology
  for (uint16_t i = 0; i < FLAC_PLUGIN_SIZE; i++) {
    // Wait for DREQ before each register write
    waitForDREQ();

    // Read address and data from PROGMEM
    uint8_t addr = pgm_read_byte(&flacPluginAddresses[i]);
    uint16_t data = pgm_read_word(&flacPluginData[i]);

    // Write to the appropriate register
    if (addr == 0x6) {
      // Write to WRAM register (most common)
      writeRegister(VS1053_REG_WRAM, data);
    } else if (addr == 0x7) {
      // Write to WRAMADDR register (sets address for subsequent WRAM writes)
      writeRegister(VS1053_REG_WRAMADDR, data);
    } else {
      // Write to other registers as specified
      writeRegister(addr, data);
    }

  }

  // Final DREQ wait to ensure plugin is fully loaded
  waitForDREQ();
  delay(100);

  LOG_CAT(VS1053, "FLAC plugin loaded successfully");
  _flacPluginLoaded = true;

  // Verify the plugin is working by checking chip status
  uint16_t status = readRegister(VS1053_REG_STATUS);
  if (!(status & 0x0040)) {
    LOG_ERROR("FLAC plugin verification failed - chip not responding");
    return false;
  }

  LOG_CAT(VS1053, "FLAC plugin verification successful");
  return true;
}

bool VS1053_driver::prepareFLACPlayback() {
  LOG_CAT(VS1053, "Preparing VS1053 for FLAC playback");

  // Load FLAC plugin if not already loaded
  if (!isFLACPluginLoaded()) {
    if (!loadFLACPlugin()) {
      LOG_ERROR("Failed to load FLAC plugin");
      return false;
    }
  }

  // Ensure FLAC clock is properly configured
  if (!configureFLACClock()) {
    LOG_ERROR("Failed to configure FLAC clock");
    return false;
  }

  LOG_CAT(VS1053, "FLAC playback preparation complete");
  return true;
}

bool VS1053_driver::isFLACPluginLoaded() {
  // The plugin lives in the chip's RAM and is lost on any reset (hardware or
  // SM_RESET), while CLOCKF survives a soft reset - so the clock value can't
  // tell whether the plugin is there. Track it ourselves.
  if (!_flacPluginLoaded) {
    LOG_CAT(VS1053, "FLAC plugin not loaded");
    return false;
  }

  // Additional verification: check if chip responds properly with FLAC clock
  uint16_t status = readRegister(VS1053_REG_STATUS);
  if (!(status & 0x0040)) {
    LOG_ERROR("FLAC plugin verification failed - chip not responding");
    return false;
  }

  LOG_CAT(VS1053, "FLAC plugin verified successfully");
  return true;
}

void VS1053_driver::writeFLACPluginData(const uint16_t *pluginData,
                                        size_t length) {
  LOG_CAT(VS1053, "Writing FLAC plugin data: %d words", length);

  for (size_t i = 0; i < length; i++) {
    uint16_t data = pgm_read_word(&pluginData[i]);
    if (data == 0x0000 && i > 0)
      break; // End marker

    writeRegister(VS1053_REG_WRAM, data);
    waitForDREQ();
  }

  LOG_CAT(VS1053, "FLAC plugin data written");
}
