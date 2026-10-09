/*!
 * @file VS1053_driver.cpp
 * Core VS1053 driver implementation
 */

#include "VS1053_driver.h"
#include "VS1053_FLAC_plugin.h"
#include "AudioProfiler.h"
#include "Spi1Bus.h"

VS1053_driver::VS1053_driver(uint8_t rst, uint8_t cs, uint8_t dcs, uint8_t dreq)
    : _rst(rst), _cs(cs), _dcs(dcs), _dreq(dreq), _useHardwareSPI(true) {}

VS1053_driver::VS1053_driver(uint8_t mosi, uint8_t miso, uint8_t sclk,
                             uint8_t rst, uint8_t cs, uint8_t dcs, uint8_t dreq)
    : _mosi(mosi), _miso(miso), _sclk(sclk), _rst(rst), _cs(cs), _dcs(dcs),
      _dreq(dreq), _useHardwareSPI(false) {}

VS1053_driver::~VS1053_driver() {}

bool VS1053_driver::begin(uint32_t spiFreq) {
  _sciFreq = spiFreq;
  _sdiFreq = spiFreq;

  pinMode(_rst, OUTPUT);
  pinMode(_cs, OUTPUT);
  pinMode(_dcs, OUTPUT);
  pinMode(_dreq, INPUT_PULLUP); // CRITICAL: Use pullup for STM32

  digitalWrite(_cs, HIGH);
  digitalWrite(_dcs, HIGH);
  _dcsPort = digitalPinToPort(_dcs);
  _dcsMask = digitalPinToBitMask(_dcs);
  _dreqPort = digitalPinToPort(_dreq);
  _dreqMask = digitalPinToBitMask(_dreq);

  if (!_useHardwareSPI) {
    pinMode(_mosi, OUTPUT);
    pinMode(_miso, INPUT);
    pinMode(_sclk, OUTPUT);
    digitalWrite(_sclk, LOW);
  }

  initSPI();
  reset();

  uint16_t status = readRegister(VS1053_REG_STATUS);
  if (!(status & 0x0040)) {
    return false;
  }

  writeRegister(VS1053_REG_MODE, VS1053_MODE_SM_SDINEW);
  writeRegister(VS1053_REG_CLOCKF, 0x6000); // 12.288MHz

  setVolume(60, 60);

  return true;
}

void VS1053_driver::initSPI() {
  if (_useHardwareSPI) {
    SPI.begin();
    // Arduino_Core_STM32 >= 3.0.0: SPI.begin() with the default 4MHz settings
    // is a no-op (configSpi() skips spi_init() because _spiSettings already
    // equals the defaults). Force the peripheral to actually initialize by
    // issuing a transaction with our (non-default) frequency.
    SPI.beginTransaction(SPISettings(_sciFreq, MSBFIRST, SPI_MODE0));
    SPI.endTransaction();
  }
}

// CLKI = XTALI x SC_MULT (SC_ADD only kicks in for WMA/AAC, so it isn't
// counted). The STM32 core rounds a requested SPI clock down to
// 90 MHz / 2^k, e.g. CLOCKF 0x6000 (CLKI 36.9 MHz) gives SCI 2.8 MHz and
// SDI 5.6 MHz; straight out of reset (CLKI 12.288 MHz) 1.4 / 2.8 MHz.
void VS1053_driver::updateClockSpeeds(uint16_t clockf) {
  static const uint8_t multX2[8] = {2, 4, 5, 6, 7, 8, 9, 10};
  uint32_t clki = VS1053_XTALI_12_288MHZ / 2 * multX2[clockf >> 13];
  _sciFreq = clki / 7;
  setSdiFreq(clki / 4);
}

// Cap on the SDI clock the feeder interrupt uses (0 = none), for testing
// signal integrity on the breadboard: -DOPENPOD_FEEDER_SDI_MAX_HZ=5625000.
#ifndef OPENPOD_FEEDER_SDI_MAX_HZ
#define OPENPOD_FEEDER_SDI_MAX_HZ 0
#endif

void VS1053_driver::setSdiFreq(uint32_t hz) {
  _sdiFreq = hz;
  if (OPENPOD_FEEDER_SDI_MAX_HZ && hz > OPENPOD_FEEDER_SDI_MAX_HZ) {
    hz = OPENPOD_FEEDER_SDI_MAX_HZ;
  }
  // SPI1 baud rate bits for the feeder interrupt, which programs SPI1
  // directly: PCLK2 / 2^(BR+1), the largest that doesn't exceed hz (same
  // rounding as the STM32 core's SPISettings).
  uint32_t pclk = HAL_RCC_GetPCLK2Freq();
  uint32_t br = 0;
  while (br < 7 && (pclk >> (br + 1)) > hz) {
    br++;
  }
  _sdiBaudBits = br << SPI_CR1_BR_Pos;
}

void VS1053_driver::reset() {
  digitalWrite(_rst, LOW);
  delay(50); // Reduced delay
  digitalWrite(_rst, HIGH);
  delay(50); // Reduced delay

  // Wait for DREQ with timeout
  unsigned long timeout = millis() + 1000;
  while (!digitalRead(_dreq) && millis() < timeout) {
    delay(1);
  }
  updateClockSpeeds(0); // CLKI = XTALI until CLOCKF is written
  _flacPluginLoaded = false;
}

void VS1053_driver::softReset() {
  writeRegister(VS1053_REG_MODE, VS1053_MODE_SM_SDINEW | VS1053_MODE_SM_RESET);
  delay(50); // Reduced delay
  _flacPluginLoaded = false; // patches are wiped by a software reset too
  // Re-derive the SPI clocks from whatever CLOCKF holds after the reset,
  // reading it at the slowest safe speed first.
  updateClockSpeeds(0);
  updateClockSpeeds(readRegister(VS1053_REG_CLOCKF));
}

// FIXED: Simplified SPI transaction management
void VS1053_driver::beginSCITransaction() {
  if (_useHardwareSPI) {
    SPI.beginTransaction(SPISettings(_sciFreq, MSBFIRST, SPI_MODE0));
  }
  controlSelect();
}

void VS1053_driver::endSCITransaction() {
  controlDeselect();
  if (_useHardwareSPI) {
    SPI.endTransaction();
  }
}

void VS1053_driver::beginSDITransaction() {
  if (_useHardwareSPI) {
    SPI.beginTransaction(SPISettings(_sdiFreq, MSBFIRST, SPI_MODE0));
  }
  dataSelect();
}

void VS1053_driver::endSDITransaction() {
  dataDeselect();
  if (_useHardwareSPI) {
    SPI.endTransaction();
  }
}

uint8_t VS1053_driver::spiTransfer(uint8_t data) {
  if (_useHardwareSPI) {
    return SPI.transfer(data);
  } else {
    uint8_t result = 0;
    for (int i = 7; i >= 0; i--) {
      digitalWrite(_sclk, LOW);
      digitalWrite(_mosi, (data >> i) & 1);
      delayMicroseconds(1);
      digitalWrite(_sclk, HIGH);
      if (digitalRead(_miso)) {
        result |= (1 << i);
      }
      delayMicroseconds(1);
    }
    digitalWrite(_sclk, LOW);
    return result;
  }
}

void VS1053_driver::spiWrite(uint8_t data) { spiTransfer(data); }
uint8_t VS1053_driver::spiRead() { return spiTransfer(0xFF); }
void VS1053_driver::controlSelect() { digitalWrite(_cs, LOW); }
void VS1053_driver::controlDeselect() { digitalWrite(_cs, HIGH); }
void VS1053_driver::dataSelect() { digitalWrite(_dcs, LOW); }
void VS1053_driver::dataDeselect() { digitalWrite(_dcs, HIGH); }

void VS1053_driver::waitForDREQ() {
  unsigned long timeout =
      millis() + 10; 
  while (!digitalRead(_dreq) && millis() < timeout) {
    // Removed yield() call to reduce overhead in audio feeding loops
  }
}

void VS1053_driver::waitForDREQFast() {
  while (!digitalRead(_dreq)) {
    // Tight loop for audio feeding - no timeout, no yield
  }
}

uint16_t VS1053_driver::readRegister(uint8_t addr) {
  // Lock before waiting on DREQ: the feeder would otherwise keep refilling
  // the chip and holding DREQ low.
  spi1bus::Lock lock;
  waitForDREQ();

  beginSCITransaction();
  spiWrite(VS1053_SCI_READ);
  spiWrite(addr);
  uint16_t result = (spiRead() << 8) | spiRead();
  endSCITransaction();

  return result;
}

void VS1053_driver::writeRegister(uint8_t addr, uint16_t data) {
  spi1bus::Lock lock; // see readRegister()
  waitForDREQ();

  beginSCITransaction();
  spiWrite(VS1053_SCI_WRITE);
  spiWrite(addr);
  spiWrite(data >> 8);
  spiWrite(data & 0xFF);
  endSCITransaction();
  if (addr == VS1053_REG_CLOCKF) {
    // The new clock only applies once the chip has settled (DREQ high).
    waitForDREQ();
    updateClockSpeeds(data);
  }
}
void VS1053_driver::sendData(const uint8_t *data, size_t len) {
  if (len == 0)
    return;
  spi1bus::Lock lock;

  // One SPI transaction for the whole send; XDCS is toggled between 32-byte
  // bursts (datasheet: DREQ high guarantees room for 32 bytes).
  if (_useHardwareSPI) {
    SPI.beginTransaction(SPISettings(_sdiFreq, MSBFIRST, SPI_MODE0));
  }
  dataDeselect(); // XDCS high before the first DREQ wait

  size_t offset = 0;
  while (offset < len) {
    uint32_t t0 = audioprof::now();
    waitForDREQFast();
    audioprof::add(audioprof::VS_DREQ_WAIT, t0);
    size_t toSend = min(len - offset, (size_t)32);
    sendBurst(data + offset, toSend);
    offset += toSend;
  }

  if (_useHardwareSPI) {
    SPI.endTransaction();
  }
}

size_t VS1053_driver::isrFeed(AudioFifo &fifo, size_t maxBytes) {
  if (!(_dreqPort->IDR & _dreqMask)) {
    return 0;
  }
  // SPI1 is idle (the caller checked the bus lock) but configured for
  // whoever used it last: switch to the SDI clock, restore it afterwards so
  // the STM32 core's cached settings stay true. Mode, bit order and frame
  // size are the same for the SD card and the VS1053; only BR differs.
  SPI_TypeDef *spi = SPI1;
  uint32_t savedCr1 = spi->CR1;
  uint32_t cr1 = (savedCr1 & ~SPI_CR1_BR_Msk) | _sdiBaudBits;
  if (cr1 != savedCr1 || !(savedCr1 & SPI_CR1_SPE)) {
    spi->CR1 = cr1 & ~SPI_CR1_SPE;
    spi->CR1 = cr1 | SPI_CR1_SPE;
  }

  size_t sent = 0;
  while (sent < maxBytes && (_dreqPort->IDR & _dreqMask)) {
    uint32_t n;
    const uint8_t *p = fifo.readPtr(n);
    if (n == 0) {
      break;
    }
    if (n > 32) {
      n = 32; // DREQ high guarantees room for 32 bytes
    }
    _dcsPort->BSRR = _dcsMask << 16; // XDCS low
    for (uint32_t i = 0; i < n; i++) {
      while (!(spi->SR & SPI_SR_TXE)) {
      }
      *(volatile uint8_t *)&spi->DR = p[i];
    }
    while (!(spi->SR & SPI_SR_TXE)) {
    }
    while (spi->SR & SPI_SR_BSY) {
    }
    _dcsPort->BSRR = _dcsMask; // XDCS high
    fifo.consume(n);
    sent += n;
  }

  // Received bytes were ignored: clear RXNE and the overrun flag so the
  // next main-loop transfer doesn't read a stale byte.
  (void)*(volatile uint8_t *)&spi->DR;
  (void)spi->SR;
  if (spi->CR1 != savedCr1) {
    spi->CR1 = savedCr1 & ~SPI_CR1_SPE;
    spi->CR1 = savedCr1;
  }
  return sent;
}

// One XDCS-framed SDI burst of at most 32 bytes. The caller holds the SPI
// transaction and has checked DREQ.
void VS1053_driver::sendBurst(const uint8_t *data, size_t len) {
  uint32_t t0 = audioprof::now();
  dataSelect();
  if (_useHardwareSPI) {
    SPI.transfer(data, nullptr, len);
  } else {
    for (size_t i = 0; i < len; i++) {
      spiWrite(data[i]);
    }
  }
  dataDeselect();
  audioprof::add(audioprof::VS_CLOCK, t0);
  audioprof::addBytes(audioprof::VS_CLOCK, len);
}

bool VS1053_driver::readyForData() { return digitalRead(_dreq); }

void VS1053_driver::setVolume(uint8_t left, uint8_t right) {
  writeRegister(VS1053_REG_VOLUME, (left << 8) | right);
}

uint16_t VS1053_driver::getVolumeRaw() { return readRegister(VS1053_REG_VOLUME); }

uint16_t VS1053_driver::getDecodeTime() {
  return readRegister(VS1053_REG_DECODETIME);
}

void VS1053_driver::resetDecodeTime() {
  // Datasheet: write 0 twice in a row to reliably clear DECODE_TIME.
  writeRegister(VS1053_REG_DECODETIME, 0);
  writeRegister(VS1053_REG_DECODETIME, 0);
}

void VS1053_driver::setPlaySpeed(uint16_t speed) {
  writeRegister(VS1053_REG_WRAMADDR, VS1053_PARA_PLAYSPEED);
  writeRegister(VS1053_REG_WRAM, speed);
}

uint16_t VS1053_driver::getPlaySpeed() {
  writeRegister(VS1053_REG_WRAMADDR, VS1053_PARA_PLAYSPEED);
  return readRegister(VS1053_REG_WRAM);
}

void VS1053_driver::sineTest(uint8_t freq, uint16_t duration) {
  spi1bus::Lock lock;
  writeRegister(VS1053_REG_MODE, VS1053_MODE_SM_SDINEW | VS1053_MODE_SM_TESTS);

  waitForDREQ();
  beginSDITransaction();

  spiWrite(0x53);
  spiWrite(0xEF);
  spiWrite(0x6E);
  spiWrite(freq);
  for (int i = 0; i < 44; i++)
    spiWrite(0x00);

  endSDITransaction();
  delay(duration);

  waitForDREQ();
  beginSDITransaction();

  spiWrite(0x45);
  spiWrite(0x78);
  spiWrite(0x69);
  spiWrite(0x74);
  for (int i = 0; i < 44; i++)
    spiWrite(0x00);

  endSDITransaction();
  writeRegister(VS1053_REG_MODE, VS1053_MODE_SM_SDINEW);
}

void VS1053_driver::cancel() {
  writeRegister(VS1053_REG_MODE, VS1053_MODE_SM_SDINEW | VS1053_MODE_SM_CANCEL);
}

bool VS1053_driver::canJump() {
  return !(readRegister(VS1053_REG_STATUS) & VS1053_STATUS_SS_DO_NOT_JUMP);
}

uint8_t VS1053_driver::getEndFillByte() {
  writeRegister(VS1053_REG_WRAMADDR, VS1053_PARA_ENDFILLBYTE);
  return readRegister(VS1053_REG_WRAM) & 0xFF;
}

void VS1053_driver::dumpRegisters() {
  Serial.println("VS1053 Registers:");
  Serial.printf("MODE: 0x%04X\n", readRegister(VS1053_REG_MODE));
  Serial.printf("STATUS: 0x%04X\n", readRegister(VS1053_REG_STATUS));
  Serial.printf("CLOCKF: 0x%04X\n", readRegister(VS1053_REG_CLOCKF));
  Serial.printf("VOLUME: 0x%04X\n", readRegister(VS1053_REG_VOLUME));
}

// OPTIMIZED: FLAC-specific clock configuration matching working implementation
bool VS1053_driver::configureFLACClock() {
  if (readRegister(VS1053_REG_CLOCKF) == VS1053_FLAC_SPEED) {
    return true; // already set by a previous FLAC start
  }
  Serial.println("Configuring VS1053 clock for FLAC playback");

  // SC_MULT 4.5x: 12.288 MHz * 4.5 = 55.3 MHz, the chip's maximum. FLAC
  // decoding is CPU-bound in the plugin, so it gets all of it.
  writeRegister(VS1053_REG_CLOCKF, VS1053_FLAC_SPEED);

  // Critical: Wait for DREQ after clock change
  waitForDREQ();
  delay(50);

  // Verify the clock configuration
  uint16_t clockf = readRegister(VS1053_REG_CLOCKF);
  if (clockf != VS1053_FLAC_SPEED) {
    Serial.printf("FLAC clock configuration failed: 0x%04X\n", clockf);
    return false;
  }

  // writeRegister() already re-derived the SPI clocks from the new CLOCKF.
  Serial.printf("FLAC clock configured: SCI %lu Hz, SDI %lu Hz\n",
                (unsigned long)_sciFreq, (unsigned long)_sdiFreq);
  return true;
}

bool VS1053_driver::configureOptimalClock(uint32_t xtalFreq) {
  Serial.printf("Configuring VS1053 clock for %lu Hz crystal\n", xtalFreq);

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
  Serial.printf("Clock configured: 0x%04X\n", clockf);

  // Now we can optimize SPI speed since internal clock is higher
  optimizeSPISpeed();

  // Verify chip is still responsive
  uint16_t status = readRegister(VS1053_REG_STATUS);
  if (!(status & 0x0040)) {
    Serial.println("Error: VS1053 not responding after clock configuration");
    return false;
  }

  Serial.printf("Max sample rate: %lu Hz\n", getMaxSampleRate());
  return true;
}

void VS1053_driver::setClockMultiplier(uint16_t scMult, uint16_t scAdd) {
  // Combine SC_MULT and SC_ADD values
  uint16_t clockf = scMult | scAdd;

  Serial.printf("Setting clock multiplier: 0x%04X\n", clockf);

  // Write to CLOCKF register
  writeRegister(VS1053_REG_CLOCKF, clockf);
}

void VS1053_driver::setClockFrequency(uint32_t xtalFreq) {
  // Calculate SC_FREQ value: (XTALI - 8000000) / 4000
  // This tells the chip the actual crystal frequency
  if (xtalFreq < 8000000UL) {
    Serial.println("Warning: Crystal frequency too low");
    return;
  }

  uint16_t scFreq = (xtalFreq - 8000000UL) / 4000UL;

  // Read current CLOCKF value to preserve SC_MULT and SC_ADD
  uint16_t currentClockf = readRegister(VS1053_REG_CLOCKF);
  uint16_t scMult = currentClockf & 0xE000; // Preserve SC_MULT
  uint16_t scAdd = currentClockf & 0x1800;  // Preserve SC_ADD

  // Combine with new SC_FREQ
  uint16_t newClockf = scMult | scAdd | (scFreq & 0x07FF);

  Serial.printf("Setting crystal frequency: %lu Hz (SC_FREQ: 0x%03X)\n",
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
  Serial.printf("VS1053 SPI: SCI %lu Hz, SDI %lu Hz\n",
                (unsigned long)_sciFreq, (unsigned long)_sdiFreq);
}

void VS1053_driver::configureClockRange(uint32_t xtalFreq) {
  // Configure SM_CLK_RANGE for 24-26MHz crystals per datasheet
  // Must be set immediately after hardware reset if using high-frequency
  // crystals

  if (xtalFreq >= 24000000UL && xtalFreq <= 26000000UL) {
    Serial.printf("Enabling SM_CLK_RANGE for %lu Hz crystal\n", xtalFreq);

    // Read current mode register
    uint16_t mode = readRegister(VS1053_REG_MODE);

    // Set SM_CLK_RANGE bit to divide XTALI by 2
    mode |= VS1053_MODE_SM_CLKRANGE;

    // Write back the mode register
    writeRegister(VS1053_REG_MODE, mode);

    // Wait for DREQ after mode change
    waitForDREQ();
    delay(10);

    Serial.println("SM_CLK_RANGE enabled - crystal frequency divided by 2");
  } else if (xtalFreq >= 12000000UL && xtalFreq <= 13000000UL) {
    Serial.printf("Using standard clock range for %lu Hz crystal\n", xtalFreq);
  } else {
    Serial.printf(
        "Warning: Crystal frequency %lu Hz is outside recommended range\n",
        xtalFreq);
  }
}

// FLAC Plugin support - plugin data is now in VS1053_FLAC_plugin.h

bool VS1053_driver::loadFLACPlugin() {
  Serial.println("Loading FLAC plugin...");

  // Check if VS1053 is ready
  if (!readyForData()) {
    Serial.println("VS1053 not ready for FLAC plugin");
    return false;
  }

  // Set clock for FLAC processing first - CRITICAL FOR FLAC PERFORMANCE
  Serial.println("Setting FLAC clock configuration...");
  if (!configureFLACClock()) {
    return false;
  }

  // Now load the actual FLAC plugin data
  Serial.printf("Loading FLAC plugin data (%d words)...\n", FLAC_PLUGIN_SIZE);

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

    // Progress indication for large plugin
    if ((i % 1000) == 0) {
      Serial.printf("Plugin loading progress: %d/%d\n", i, FLAC_PLUGIN_SIZE);
    }
  }

  // Final DREQ wait to ensure plugin is fully loaded
  waitForDREQ();
  delay(100);

  Serial.println("FLAC plugin loaded successfully");
  _flacPluginLoaded = true;

  // Verify the plugin is working by checking chip status
  uint16_t status = readRegister(VS1053_REG_STATUS);
  if (!(status & 0x0040)) {
    Serial.println("FLAC plugin verification failed - chip not responding");
    return false;
  }

  Serial.println("FLAC plugin verification successful");
  return true;
}

bool VS1053_driver::prepareFLACPlayback() {
  Serial.println("Preparing VS1053 for FLAC playback");

  // Load FLAC plugin if not already loaded
  if (!isFLACPluginLoaded()) {
    if (!loadFLACPlugin()) {
      Serial.println("Failed to load FLAC plugin");
      return false;
    }
  }

  // Ensure FLAC clock is properly configured
  if (!configureFLACClock()) {
    Serial.println("Failed to configure FLAC clock");
    return false;
  }

  Serial.println("FLAC playback preparation complete");
  return true;
}

bool VS1053_driver::isFLACPluginLoaded() {
  // The plugin lives in the chip's RAM and is lost on any reset (hardware or
  // SM_RESET), while CLOCKF survives a soft reset - so the clock value can't
  // tell whether the plugin is there. Track it ourselves.
  if (!_flacPluginLoaded) {
    Serial.println("FLAC plugin not loaded");
    return false;
  }

  // Additional verification: check if chip responds properly with FLAC clock
  uint16_t status = readRegister(VS1053_REG_STATUS);
  if (!(status & 0x0040)) {
    Serial.println("FLAC plugin verification failed - chip not responding");
    return false;
  }

  Serial.println("FLAC plugin verified successfully");
  return true;
}

void VS1053_driver::writeFLACPluginData(const uint16_t *pluginData,
                                        size_t length) {
  Serial.printf("Writing FLAC plugin data: %d words\n", length);

  for (size_t i = 0; i < length; i++) {
    uint16_t data = pgm_read_word(&pluginData[i]);
    if (data == 0x0000 && i > 0)
      break; // End marker

    writeRegister(VS1053_REG_WRAM, data);
    waitForDREQ();
  }

  Serial.println("FLAC plugin data written");
}
