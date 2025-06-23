#include "PSRAM_controller.hpp"

// Define static member variables
uint8_t SPI_PSRAM::dmaWriteBuf[256] __attribute__((aligned(4)));
uint8_t SPI_PSRAM::dmaReadBuf[256] __attribute__((aligned(4)));