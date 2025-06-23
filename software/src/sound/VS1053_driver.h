/*!
 * @file VS1053_driver.h
 * Core VS1053 driver with direct SPI communication
 */

#ifndef VS1053_DRIVER_H
#define VS1053_DRIVER_H

#include "VS1053_FLAC_plugin.h"
#include <Arduino.h>
#include <SPI.h>

// VS1053 Register Definitions
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
#define VS1053_REG_AIADDR 0x0A
#define VS1053_REG_VOLUME 0x0B
#define VS1053_REG_AICTRL0 0x0C
#define VS1053_REG_AICTRL1 0x0D
#define VS1053_REG_AICTRL2 0x0E
#define VS1053_REG_AICTRL3 0x0F

// SCI Commands
#define VS1053_SCI_READ 0x03
#define VS1053_SCI_WRITE 0x02

// Mode register bits
#define VS1053_MODE_SM_DIFF 0x0001
#define VS1053_MODE_SM_LAYER12 0x0002
#define VS1053_MODE_SM_RESET 0x0004
#define VS1053_MODE_SM_CANCEL 0x0008
#define VS1053_MODE_SM_EARSPEAKER_LO 0x0010
#define VS1053_MODE_SM_TESTS 0x0020
#define VS1053_MODE_SM_STREAM 0x0040
#define VS1053_MODE_SM_EARSPEAKER_HI 0x0080
#define VS1053_MODE_SM_DACT 0x0100
#define VS1053_MODE_SM_SDIORD 0x0200
#define VS1053_MODE_SM_SDISHARE 0x0400
#define VS1053_MODE_SM_SDINEW 0x0800
#define VS1053_MODE_SM_ADPCM 0x1000
#define VS1053_MODE_SM_ADCPM_HP 0x2000
#define VS1053_MODE_SM_LINE_IN 0x4000
#define VS1053_MODE_SM_CLKRANGE 0x8000

// Clock multiplier definitions
#define VS1053_SC_MULT_1_0X 0x0000
#define VS1053_SC_MULT_2_0X 0x2000
#define VS1053_SC_MULT_2_5X 0x4000
#define VS1053_SC_MULT_3_0X 0x6000
#define VS1053_SC_MULT_3_5X 0x8000
#define VS1053_SC_MULT_4_0X 0xA000
#define VS1053_SC_MULT_4_5X 0xC000
#define VS1053_SC_MULT_5_0X 0xE000

// Additional multiplier for WMA/AAC
#define VS1053_SC_ADD_NONE 0x0000
#define VS1053_SC_ADD_1_0X 0x0800
#define VS1053_SC_ADD_1_5X 0x1000
#define VS1053_SC_ADD_2_0X 0x1800

// Crystal frequencies
#define VS1053_XTALI_12_288MHZ 12288000UL
#define VS1053_XTALI_24_576MHZ 24576000UL

// SPI Speed limits
#define VS1053_MAX_SPI_SDI 7000000UL // 7MHz max for SDI at 1.0x clock

// WRAM parameter addresses
#define VS1053_PARA_PLAYSPEED 0x1E04
#define VS1053_PARA_ENDFILLBYTE 0x1E06
#define VS1053_FLAC_SPEED 0x8800
// Optimized transfer sizes
#define VS1053_BURST_SIZE 64 // Increased from 32 for better throughput

class VS1053_driver {
public:
  // Constructors
  VS1053_driver(uint8_t rst, uint8_t cs, uint8_t dcs, uint8_t dreq);
  VS1053_driver(uint8_t mosi, uint8_t miso, uint8_t sclk, uint8_t rst,
                uint8_t cs, uint8_t dcs, uint8_t dreq);
  ~VS1053_driver();

  // Core initialization and control
  bool begin(uint32_t spiFreq = 4000000UL);
  void reset();
  void softReset();
  bool readyForData();

  // Register access
  uint16_t readRegister(uint8_t addr);
  void writeRegister(uint8_t addr, uint16_t data);

  void sendData(const uint8_t *data, size_t len);

  // Audio control
  void setVolume(uint8_t left, uint8_t right);
  uint16_t getDecodeTime();
  void setPlaySpeed(uint16_t speed);
  uint16_t getPlaySpeed();
  void cancel();

  // Test functions
  void sineTest(uint8_t freq, uint16_t duration);
  void dumpRegisters();

  uint8_t getDREQPin() const { return _dreq; }
  // OPTIMIZED: Clock configuration for FLAC
  bool configureOptimalClock(uint32_t xtalFreq = VS1053_XTALI_12_288MHZ);
  void setClockMultiplier(uint16_t scMult, uint16_t scAdd);
  void setClockFrequency(uint32_t xtalFreq);
  uint32_t getMaxSampleRate();
  void optimizeSPISpeed();
  void configureClockRange(uint32_t xtalFreq);
  
  bool loadFLACPlugin();
  bool configureFLACClock(); 
  bool prepareFLACPlayback();
  bool isFLACPluginLoaded();
  void writeFLACPluginData(const uint16_t *pluginData, size_t length);

private:
  // Pin assignments
  uint8_t _rst, _cs, _dcs, _dreq;
  uint8_t _mosi, _miso, _sclk;
  bool _useHardwareSPI;
  uint32_t _spiFreq;

  // Low-level SPI functions
  void initSPI();
  uint8_t spiTransfer(uint8_t data);
  void spiWrite(uint8_t data);
  uint8_t spiRead();

  // Control pin functions
  void controlSelect();
  void controlDeselect();
  void dataSelect();
  void dataDeselect();

  // OPTIMIZED: DREQ handling
  void waitForDREQ();     // Standard DREQ wait with timeout
  void waitForDREQFast(); // NEW: Fast DREQ for audio streaming

  // SPI transaction management
  void beginSCITransaction();
  void endSCITransaction();
  void beginSDITransaction();
  void endSDITransaction();
};

#endif // VS1053_DRIVER_H