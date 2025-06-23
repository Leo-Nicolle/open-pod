/*!
 * @file VS1053_driver.h
 * Core VS1053 driver with direct SPI communication
 */

#ifndef VS1053_DRIVER_H
#define VS1053_DRIVER_H

#include <Arduino.h>
#include <SPI.h>
#include "VS1053_FLAC_plugin.h"

// VS1053 Register definitions
#define VS1053_SCI_READ 0x03
#define VS1053_SCI_WRITE 0x02

#define VS1053_REG_MODE 0x00
#define VS1053_REG_STATUS 0x01
#define VS1053_REG_BASS 0x02
#define VS1053_REG_CLOCKF 0x03
#define VS1053_REG_DECODETIME 0x04
#define VS1053_REG_AUDATA 0x05
#define VS1053_REG_WRAM 0x06
#define VS1053_REG_WRAMADDR 0x07
#define VS1053_REG_HDAT0 0x08
#define VS1053_REG_HDAT1 0x09
#define VS1053_REG_VOLUME 0x0B

// Mode register bits
#define VS1053_MODE_SM_DIFF 0x0001
#define VS1053_MODE_SM_LAYER12 0x0002
#define VS1053_MODE_SM_RESET 0x0004
#define VS1053_MODE_SM_CANCEL 0x0008
#define VS1053_MODE_SM_EARSPKLO 0x0010
#define VS1053_MODE_SM_TESTS 0x0020
#define VS1053_MODE_SM_STREAM 0x0040
#define VS1053_MODE_SM_SDINEW 0x0800
#define VS1053_MODE_SM_ADPCM 0x1000
#define VS1053_MODE_SM_LINE1 0x4000
#define VS1053_MODE_SM_CLKRANGE 0x8000

#define VS1053_PARA_PLAYSPEED 0x1E04

// FIFO and data transmission constants
#define VS1053_FIFO_SIZE 2048        // VS1053b FIFO is 2048 bytes
#define VS1053_BURST_SIZE 32         // Optimal burst size per datasheet
#define VS1053_MAX_SPI_SDI 7000000UL // Max SDI SPI speed at 1.0x clock

// Clock multiplier definitions (SC_MULT values)
#define VS1053_SC_MULT_1_0X   0x0000  // XTALI × 1.0
#define VS1053_SC_MULT_2_0X   0x2000  // XTALI × 2.0
#define VS1053_SC_MULT_2_5X   0x4000  // XTALI × 2.5
#define VS1053_SC_MULT_3_0X   0x6000  // XTALI × 3.0
#define VS1053_SC_MULT_3_5X   0x8000  // XTALI × 3.5
#define VS1053_SC_MULT_4_0X   0xa000  // XTALI × 4.0
#define VS1053_SC_MULT_4_5X   0xc000  // XTALI × 4.5
#define VS1053_SC_MULT_5_0X   0xe000  // XTALI × 5.0

// Clock addition definitions (SC_ADD values)
#define VS1053_SC_ADD_NONE    0x0000  // No modification allowed
#define VS1053_SC_ADD_1_0X    0x0800  // XTALI × 1.0 addition
#define VS1053_SC_ADD_1_5X    0x1000  // XTALI × 1.5 addition
#define VS1053_SC_ADD_2_0X    0x1800  // XTALI × 2.0 addition

// Clock frequency configurations for common crystals
#define VS1053_XTALI_12_288MHZ  12288000UL
#define VS1053_XTALI_24_576MHZ  24576000UL

/*!
 * @brief Core VS1053 driver with direct SPI communication
 */
class VS1053_driver {
public:
  VS1053_driver(uint8_t rst, uint8_t cs, uint8_t dcs, uint8_t dreq);
  VS1053_driver(uint8_t mosi, uint8_t miso, uint8_t sclk, uint8_t rst, uint8_t cs, uint8_t dcs, uint8_t dreq);
  ~VS1053_driver();

  bool begin(uint32_t spiFreq = 1000000);
  void reset();
  void softReset();
  uint16_t readRegister(uint8_t addr);
  void writeRegister(uint8_t addr, uint16_t data);
  void sendData(const uint8_t* data, size_t len);
  bool readyForData();
  void setVolume(uint8_t left, uint8_t right);
  uint16_t getDecodeTime();
  void setPlaySpeed(uint16_t speed);
  uint16_t getPlaySpeed();
  void sineTest(uint8_t freq, uint16_t duration);
  void cancel();
  void dumpRegisters();

  // Enhanced clock configuration methods
  bool configureOptimalClock(uint32_t xtalFreq = VS1053_XTALI_12_288MHZ);
  void setClockMultiplier(uint16_t scMult, uint16_t scAdd = VS1053_SC_ADD_NONE);
  void setClockFrequency(uint32_t xtalFreq);
  void configureClockRange(uint32_t xtalFreq);
  uint32_t getMaxSampleRate();
  void optimizeSPISpeed();
  
  // FLAC support methods
  bool loadFLACPlugin();
  bool isFLACPluginLoaded();
  void writeFLACPluginData(const uint16_t* pluginData, size_t length);
  
  // Enhanced data transmission methods
  void sendDataBurst(const uint8_t* data, size_t len);

  // Pin access for interrupt management
  uint8_t getDREQPin() const { return _dreq; }

protected:
  uint8_t _rst, _cs, _dcs, _dreq;
  uint8_t _mosi, _miso, _sclk;
  bool _useHardwareSPI;
  uint32_t _spiFreq;
  
  void initSPI();
  void beginSCITransaction();
  void endSCITransaction();
  void beginSDITransaction();
  void endSDITransaction();
  uint8_t spiTransfer(uint8_t data);
  void spiWrite(uint8_t data);
  uint8_t spiRead();
  
  void waitForDREQ();
  void controlSelect();
  void controlDeselect();
  void dataSelect();
  void dataDeselect();
};

#endif // VS1053_DRIVER_H