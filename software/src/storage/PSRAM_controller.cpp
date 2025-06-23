#include "PSRAM_controller.hpp"


// Static member definitions
volatile bool SPI_PSRAM::dmaTransferComplete = true;
volatile HAL_StatusTypeDef SPI_PSRAM::lastDmaStatus = HAL_OK;
SPI_PSRAM* SPI_PSRAM::instance = nullptr;
uint8_t SPI_PSRAM::dmaWriteBuf[DMA_BUFFER_SIZE] __attribute__((aligned(4)));
uint8_t SPI_PSRAM::dmaReadBuf[DMA_BUFFER_SIZE] __attribute__((aligned(4)));
