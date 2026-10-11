/*!
 * @file VS1053_driver.cpp
 * Core VS1053 driver implementation (clock and FLAC plugin setup are in
 * VS1053_driver_config.cpp)
 */

#include "VS1053_driver.h"
#include "AudioProfiler.h"
#include "Spi1Bus.h"
#include "../logging.h"

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

  // Raise XRESET before making it an output: the output latch starts low, so
  // pinMode() alone would pulse a hardware reset (see below).
  digitalWrite(_rst, HIGH);
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
  // A hardware reset powers the analog outputs down and back up, which
  // thumps in headphones like a jack pulled out and plugged back in. The chip
  // already reset itself at power-up, so when it answers, a software reset
  // (outputs stay up) is enough; the reset pin is only the fallback.
  updateClockSpeeds(0); // slowest SCI speed: CLOCKF may not be set yet
  if (isVS1053(readRegister(VS1053_REG_STATUS))) {
    softReset();
  } else {
    reset();
  }

  uint16_t status = readRegister(VS1053_REG_STATUS);
  if (!isVS1053(status)) {
    return false;
  }
  // Quietest setting that keeps the analog drivers powered (0xFFFF would
  // power them down, and powering back up pops). The caller sets the volume.
  setVolume(0xFE, 0xFE);

  writeRegister(VS1053_REG_MODE, VS1053_MODE_SM_SDINEW);
  writeRegister(VS1053_REG_CLOCKF, 0x6000); // 12.288MHz

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

// SS_VER (status bits 7:4) is 4 for the VS1053; floating MISO reads 0xFFFF.
bool VS1053_driver::isVS1053(uint16_t status) { return ((status >> 4) & 0xF) == 4; }

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
  applyI2SOutput(); // a hardware reset turns I2S off
}

void VS1053_driver::softReset() {
  writeRegister(VS1053_REG_MODE, VS1053_MODE_SM_SDINEW | VS1053_MODE_SM_RESET);
  delay(50); // Reduced delay
  _flacPluginLoaded = false; // patches are wiped by a software reset too
  // Re-derive the SPI clocks from whatever CLOCKF holds after the reset,
  // reading it at the slowest safe speed first.
  updateClockSpeeds(0);
  updateClockSpeeds(readRegister(VS1053_REG_CLOCKF));
  applyI2SOutput(); // a software reset clears GPIO_DDR, so I2S too
}

void VS1053_driver::setI2SOutput(bool enabled) {
  _i2sOutput = enabled;
  if (enabled) {
    applyI2SOutput();
  } else {
    writeRegister(VS1053_REG_WRAMADDR, VS1053_I2S_CONFIG);
    writeRegister(VS1053_REG_WRAM, 0);
  }
}

// Datasheet 11.14: GPIO4-7 as outputs, then I2S_CONFIG. 0x4 = enabled at
// 48 kHz: SCLK = 12.288 MHz / 8 = 1.536 MHz, i.e. 16-bit words, which is what
// the FSC-BT1035 expects with AT+I2SCFG=3 (see Bluetooth.hpp). The rate bits
// can only change while I2S is disabled, hence the 0 first.
void VS1053_driver::applyI2SOutput() {
  if (!_i2sOutput)
    return;
  writeRegister(VS1053_REG_WRAMADDR, VS1053_I2S_CONFIG);
  writeRegister(VS1053_REG_WRAM, 0);
  writeRegister(VS1053_REG_WRAMADDR, VS1053_GPIO_DDR);
  writeRegister(VS1053_REG_WRAM, 0x00F0);
  writeRegister(VS1053_REG_WRAMADDR, VS1053_I2S_CONFIG);
  writeRegister(VS1053_REG_WRAM, 0x0004);
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
  LOG_CAT(VS1053, "VS1053 Registers:");
  LOG_CAT(VS1053, "MODE: 0x%04X", readRegister(VS1053_REG_MODE));
  LOG_CAT(VS1053, "STATUS: 0x%04X", readRegister(VS1053_REG_STATUS));
  LOG_CAT(VS1053, "CLOCKF: 0x%04X", readRegister(VS1053_REG_CLOCKF));
  LOG_CAT(VS1053, "VOLUME: 0x%04X", readRegister(VS1053_REG_VOLUME));
}
