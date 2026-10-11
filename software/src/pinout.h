#pragma once

// VS1053 and SD card
#define CARDCS     PA4  // Card chip select pin
#define MP3CS      PA11 // VS1053 chip select pin
#define MP3DREQ    PB3 // VS1053 data request pin
#define MP3XDCS    PA9  // VS1053 data/command select pin
#define MP3_RESET  PA10 // VS1053 reset pin


// PSRAM SPI interface
#define PSRAM_CS PD2 // moved off PB6 to free USART1 (PB6/PB7) for Bluetooth
#define PSRAM_CLK PB13
#define PSRAM_MISO PB14
#define PSRAM_MOSI PB15

// I2C3 bus: shared by the MPR121 and the DRV2605L
#define I2C3_SDA PB4
#define I2C3_SCL PA8

// MPR121 touch wheel (I2C3: SDA PB4, SCL PA8). ADDR pin tied to 3.3V -> 0x5B,
// because the DRV2605L on the same bus has a fixed 0x5A address.
#define MPR121_ADDR 0x5B
// MPR121 ~IRQ: open-drain, active low, held low until the touch status is
// read. PH1 is free on the Nucleo: the clock runs from HSI, so OSC_OUT is unused
// (morpho CN7-31). No external pull-up on the line -> INPUT_PULLUP.
#define MPR121_IRQ PH1

// DRV2605L haptic driver: shares I2C3 with the MPR121, fixed address 0x5A
#define DRV2605_ADDR 0x5A

// Bluetooth FSC-BT1035 (USART1, 115200 8N1, AT commands)
#define BT_UART_TX PB6 // STM32 TX -> BT_RX (module pin 14)
#define BT_UART_RX PB7 // STM32 RX <- BT_TX (module pin 13)
#define BT_SYS_CTRL PA15 // power-on -> SYS_CTRL (module pin 34), high > 20 ms boots

// Click-wheel mechanical buttons (Alps SKQGAKE010), active low, INPUT_PULLUP
#define BTN_TOP    PB5
#define BTN_RIGHT  PB1
#define BTN_BOTTOM PB0
#define BTN_LEFT   PB2
#define BTN_CENTER PA12
