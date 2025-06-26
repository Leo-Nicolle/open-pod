#include "PSRAM_controller.hpp"


// Static member definitions
volatile bool SPI_PSRAM::dmaTransferComplete = true;
volatile HAL_StatusTypeDef SPI_PSRAM::lastDmaStatus = HAL_OK;
SPI_PSRAM* SPI_PSRAM::instance = nullptr;
uint8_t SPI_PSRAM::dmaWriteBuf[DMA_BUFFER_SIZE] __attribute__((aligned(4)));
uint8_t SPI_PSRAM::dmaReadBuf[DMA_BUFFER_SIZE] __attribute__((aligned(4)));
SPI_PSRAM psram;


// 🔧 HAL Callback Functions - À ajouter dans main.cpp
extern "C" {
  void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (SPI_PSRAM::instance) {
      SPI_PSRAM::instance->dmaCompleteCallback(HAL_OK);
    }
  }
  
  void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (SPI_PSRAM::instance) {
      SPI_PSRAM::instance->dmaCompleteCallback(HAL_OK);
    }
  }
  
  void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (SPI_PSRAM::instance) {
      SPI_PSRAM::instance->dmaCompleteCallback(HAL_OK);
    }
  }
  
  void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    if (SPI_PSRAM::instance) {
      SPI_PSRAM::instance->dmaCompleteCallback(HAL_ERROR);
    }
  }
  
  // 🔧 DMA Interrupt Handlers
  void DMA1_Stream3_IRQHandler(void) {
    if (SPI_PSRAM::instance) {
      HAL_DMA_IRQHandler(&(SPI_PSRAM::instance->hdma_spi_rx));
    }
  }
  
  void DMA1_Stream4_IRQHandler(void) {
    if (SPI_PSRAM::instance) {
      HAL_DMA_IRQHandler(&(SPI_PSRAM::instance->hdma_spi_tx));
    }
  }
}