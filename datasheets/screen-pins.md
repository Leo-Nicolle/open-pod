# Screen:

[Aliexpress link](https://fr.aliexpress.com/item/1005005884190356.html?spm=a2g0o.store_pc_allItems_or_groupList.0.0.522d56996ucBe9&pdp_npi=4%40dis%21EUR%21%E2%82%AC%202%2C99%21%E2%82%AC%202%2C99%21%21%213.30%213.30%21%40211b6a7a17481776068018275e5dca%2112000034697320452%21sh%21FR%211709402015%21X&_gl=1*1ox6i3u*_gcl_aw*R0NMLjE3NDc5MTI1MjUuQ2owS0NRandscnZCQmhEbkFSSXNBSEVRZ09SVExIenk2X0txa0NzSXI1X2lueUFpcGtCSVdBOHlMWTR5a1oxYjRaUjFFTHU0UHdlZ1p4WWFBbnNWRUFMd193Y0I.*_gcl_dc*R0NMLjE3NDc5MTI1MjUuQ2owS0NRandscnZCQmhEbkFSSXNBSEVRZ09SVExIenk2X0txa0NzSXI1X2lueUFpcGtCSVdBOHlMWTR5a1oxYjRaUjFFTHU0UHdlZ1p4WWFBbnNWRUFMd193Y0I.*_gcl_ag*Mi4xLmswQUFBQUFEaWhocVZuRnlMNm9SSzNSQ0F3TUgwdENJZ1BEJGkxNzQ3NDg2Nzc2*_gcl_au*MTQyNTU4NDQ1Mi4xNzQ1Nzc1NDgz*_ga*NTM3MTMzMDgxLjE3Mjk4ODYzODk.*_ga_VED1YSGNC7*czE3NDgxNzc1NjkkbzQ3JGcxJHQxNzQ4MTc3NjA4JGoyMSRsMCRoMCRkcjFnQUNDblVvTmM5MDN3MUpIcGxrX1l3ejB5eHNhTzFnQQ..&gatewayAdapt=glo2fra)

- 40pins 0.5mm pitch
- 2.4inch
- ILI9341
- 240x320 pixels
- 3.3V
- 8 bits and 16 bits parallel interface
- HSD2.4

| FPC | REVERSE | FUNCTION        | STM32F4         |
| --- | ------- | --------------- | --------------- |
| 1   | 40      | X- (touch)      | ignored         |
| 2   | 39      | Y- (touch)      | ignored         |
| 3   | 38      | X+ (touch)      | ignored         |
| 4   | 37      | Y+ (touch)      | ignored         |
| 5   | 36      | GND             | GND             |
| 6   | 35      | VDDI (2.8-3.3V) | 3.3V            |
| 7   | 34      | VDD (2.8-3.3V)  | 3.3V            |
| 8   | 33      | NC/FMARK (TE)   | ignored         |
| 9   | 32      | CSX (SPI CS)    | GND             |
| 10  | 31      | DCX (DC,A0)     | PB8             |
| 11  | 30      | WRX             | PB9             |
| 12  | 29      | RDX             | PB10            |
| 13  | 28      | SPI SDI (MOSI)  | ignored         |
| 14  | 27      | SPI SDO (MISO)  | ignored         |
| 15  | 26      | RESX (RESET)    | PB12            |
| 16  | 25      | GND             | GND             |
| 17  | 24      | DB0             | PC0             |
| 18  | 23      | DB1             | PC1             |
| 19  | 22      | DB2             | PC2             |
| 20  | 21      | DB3             | PC3             |
| 21  | 20      | DB4             | PC4             |
| 22  | 19      | DB5             | PC5             |
| 23  | 18      | DB6             | PC6             |
| 24  | 17      | DB7             | PC7             |
| 25  | 16      | DB8             | PC8             |
| 26  | 15      | DB9             | PC9             |
| 27  | 14      | DB10            | PC10            |
| 28  | 13      | DB11            | PC11            |
| 29  | 12      | DB12            | PC12            |
| 30  | 11      | DB13            | PC13            |
| 31  | 10      | DB14            | PC14            |
| 32  | 9       | DB15            | PC15            |
| 33  | 8       | LED-A           | 3.3V            |
| 34  | 7       | LED-K           | pulldown 10 ohm |
| 35  | 6       | LED-K           | pulldown 10 ohm |
| 36  | 5       | LED-K           | pulldown 10 ohm |
| 37  | 4       | GND             | GND             |
| 38  | 3       | IM0             | GND             |
| 39  | 2       | IM1             | GND             |
| 40  | 1       | IM2             | GND             |

### Availiable on Nucleo-F4

- PA0
- PA1
- PA2
- PA3
- PA4
- PA5
- PA6
- PA7
- PA8
- PA9
- PA10
- PA11
- PA12
- PA13
- PA14
- PA15
- PB0
- PB1
- PB2
- PB3
- PB4
- PB5
- PB6
- PB8
- PB9
- PB10
- PB12
- PB13
- PB14
- PB15
- PC0
- PC1
- PC2
- PC3
- PC4
- PC5
- PC6
- PC7
- PC8
- PC9
- PC10
- PC11
- PC12
- PC13
- PC14
- PC15
- PD2
- PH0
- PH1

### VS1053

| Board Pin | STM32F4 Pin | Function  |
| --------- | ----------- | --------- |
| RST       | PA10        | Reset pin |
| SCK       | PA5         | SCK       |
| MOSI      | PA7         | MOSI      |
| MISO      | PA6         | MISO      |
| MP3CS     | PA11        | MP3 CS    |
| SDCS      | PA4         | SD CS     |
| DREQ      | PB3         | Dreq      |
| XDCS      | PA9         | A0        |

## MPR121 Capacitive Touch Sensor

| MPR121 Pin | Function  | STM32F446RE Pin | Notes                  |
| ---------- | --------- | --------------- | ---------------------- |
| VIN        | Power     | 3.3V            | 2.8V-5V power supply   |
| GND        | Ground    | GND             | Ground connection      |
| SCL        | Clock     | PA8 (I2C3_SCL)  | I2C clock line         |
| SDA        | Data      | PB4 (I2C3_SDA)  | I2C data line          |
| IRQ        | Interrupt | PA12            | Optional interrupt pin |

### PSRAM

| APS6404L Pin   | Function      | STM32F446RE Pin | Notes                             |
| -------------- | ------------- | --------------- | --------------------------------- |
| 1 (~CE)        | Chip Enable   | PA1 (SPI1_NSS)  | Active low chip select            |
| 2 (SO/SIO1)    | Data Output   | PA6 (SPI1_MISO) | Serial data output                |
| 3 (~WP/SIO2)   | Write Protect | 3.3V            | Tie high to disable write protect |
| 4 (VSS)        | Ground        | GND             | Ground connection                 |
| 5 (SI/SIO0)    | Data Input    | PA7 (SPI1_MOSI) | Serial data input                 |
| 6 (CLK)        | Clock         | PA5 (SPI1_SCK)  | SPI clock                         |
| 7 (~HOLD/SIO3) | Hold          | 3.3V            | Tie high to disable hold          |
| 8 (VCC)        | Power         | 3.3V            | 3.3V power supply                 |

### Interface command selection

| IM2 | IM1 | IM0 | Mode d'interface      |
| --- | --- | --- | --------------------- |
| 0   | 0   | 0   | 3-wire SPI (no DCX)   |
| 0   | 0   | 1   | 4-wire SPI (with DCX) |
| 0   | 1   | 1   | 16-bit Parallel       |
| 1   | 1   | 1   | 8-bit Parallel        |

### STM32F4 reversed
