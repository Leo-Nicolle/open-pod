#pragma once

// VS1053 and SD card
#define CARDCS     PA4  // Card chip select pin
#define MP3CS      PA11 // VS1053 chip select pin
#define MP3DREQ    PB3 // VS1053 data request pin
#define MP3XDCS    PA9  // VS1053 data/command select pin
#define MP3_RESET  PA10 // VS1053 reset pin


// PSRAM SPI interface
#define PSRAM_CS PB6
#define PSRAM_CLK PB13
#define PSRAM_MISO PB14
#define PSRAM_MOSI PB15
