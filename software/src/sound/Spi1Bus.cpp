#include "Spi1Bus.h"

volatile uint8_t spi1bus::lockCount = 0;
volatile bool spi1bus::feedPending = false;
void (*volatile spi1bus::onFreed)() = nullptr;
