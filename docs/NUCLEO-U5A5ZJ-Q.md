# NUCLEO-U5A5ZJ-Q reference

Reference notes for the STM32U5A5ZJT6Q MCU on the ST NUCLEO-U5A5ZJ-Q board (MB1549
reference design), the candidate replacement for the current MCU in this project.

Sources: `datasheets/nucleo144-boards.pdf` (UM2861 rev 10, covers NUCLEO-U575ZI-Q and
NUCLEO-U5A5ZJ-Q, both built on the same MB1549 board) and `datasheets/DS_stm32u5a5zj.pdf`
(DS13543 rev 3).

**Package**: the "Z" in `STM32U5A5ZJT6Q` = LQFP144. The trailing **Q** = "dedicated
pinout supporting internal SMPS step-down converter", so this exact part is the
**LQFP144_SMPS** pinout variant (not the plain LQFP144). All pin numbers below are the
LQFP144_SMPS column of the datasheet's pin table — this is the pinout that matches the
physical MB1549 board's silkscreen/pin-1 marking.

## 1. Full pin map

144 pins total. **Nucleo connector** column gives every physical position the pin is
exposed on:
- `CN11.n` / `CN12.n` = ST Morpho male headers (not populated by default), which expose
  *every* STM32 signal and power pin — this is the authoritative "is this pin present on
  the board" answer.
- `CNx.n(Dn/An)` = ARDUINO-compatible Zio connectors (CN7–CN10), only populated for the
  subset of pins that also carry an Arduino Uno-style `Dn`/`An` label.
- `-` = internal supply/ground pin not brought out to a connector pin (referenced only
  by copper plane / decoupling network on the board), or a signal pin whose only
  connector row happens to be a shared power rail.

Alternate functions are listed in AF-table order (AF0 first) with no AF index shown,
since ST's own table groups them positionally, not numerically, in the datasheet text
extracted here — cross-check against `DS_stm32u5a5zj.pdf` Table 28 (AF0–AF7) / Table 29
(AF8–AF15) if you need the exact `GPIOx_AFRy` value for a given function; "Additional
functions" are enabled directly via peripheral registers, not `AFR`.

| # | Pin name | Type | I/O structure | Nucleo connector | Alternate functions (AF0→AF15) | Additional functions | Notes |
|---|---|---|---|---|---|---|---|
| 1 | PE2 | I/O | FT_ha | CN11.46, CN9.14(D56) | TRACECLK, TIM3_ETR, SAI1_CK1, USART6_CK, LCD_R0, TSC_G7_IO1, LPGPIO1_P14, FMC_A23, SAI1_MCLK_A, EVENTOUT | - | - |
| 2 | PE3 | I/O | FT_hat | CN11.47, CN9.22(D60) | TRACED0, TIM3_CH1, OCTOSPIM_P1_DQS, USART6_CTS, LCD_R1, TSC_G7_IO2, LPGPIO1_P15, FMC_A19, SAI1_SD_B, EVENTOUT | TAMP_IN6/TAMP_OUT3 | - |
| 3 | PE4 | I/O | FT_hat | CN11.48, CN9.16(D57) | TRACED1, TIM3_CH2, SAI1_D2, MDF1_SDI3, USART6_RTS_DE, LCD_B0, TSC_G7_IO3, DCMI_D4/PSSI_D4, FMC_A20, SAI1_FS_A, EVENTOUT | WKUP1, TAMP_IN7/TAMP_OUT8 | - |
| 4 | PE5 | I/O | FT_hat | CN11.50, CN9.18(D58) | TRACED2, TIM3_CH3, SAI1_CK2, MDF1_CKI3, LCD_G0, TSC_G7_IO4, DCMI_D6/PSSI_D6, FMC_A21, SAI1_SCK_A, EVENTOUT | WKUP2, TAMP_IN8/TAMP_OUT7 | - |
| 5 | PE6 | I/O | FT_ht | CN11.62, CN9.20(D59) | TRACED3, TIM3_CH4, SAI1_D1, LCD_G1, DCMI_D7/PSSI_D7, FMC_A22, SAI1_SD_A, EVENTOUT | WKUP3, TAMP_IN3/TAMP_OUT6 | - |
| 6 | VBAT | S | - | CN11.33 | - | - | - |
| 7 | PC13 | I/O | FT | CN11.23 | EVENTOUT | WKUP2, RTC_TS/RTC_OUT1, TAMP_IN1/TAMP_OUT2 | (2) (3) |
| 8 | PC14-OSC32_IN (PC14) | I/O | FT_o | CN11.25 | EVENTOUT | OSC32_IN | (2) (3) |
| 9 | PC15-OSC32_OUT (PC15) | I/O | FT_o | CN11.27 | EVENTOUT | OSC32_OUT | (2) (3) |
| 10 | PF0 | I/O | FT_fh | CN11.53, CN9.21(D68) | I2C6_SDA, I2C2_SDA, OCTOSPIM_P2_IO0, USART6_TX, FMC_A0, EVENTOUT | - | - |
| 11 | PF1 | I/O | FT_fh | CN11.51, CN9.19(D69) | I2C6_SCL, I2C2_SCL, OCTOSPIM_P2_IO1, USART6_RX, FMC_A1, EVENTOUT | - | - |
| 12 | PF2 | I/O | FT_h | CN11.52, CN9.17(D70) | LPTIM3_CH2, I2C2_SMBA, OCTOSPIM_P2_IO2, USART6_CK, FMC_A2, EVENTOUT | WKUP8 | - |
| 13 | PF3 | I/O | FT_h | CN12.58, CN8.14(D49) | LPTIM3_IN1, ADF1_CCK0, OCTOSPIM_P2_IO3, MDF1_CCK0, USART6_CTS, UART5_TX, FMC_A3, EVENTOUT | - | - |
| 14 | PF4 | I/O | FT_hvp | CN12.38 | LPTIM3_ETR, ADF1_SDI0, OCTOSPIM_P2_CLK, MDF1_SDI0, USART6_RTS_DE, UART5_RX, FMC_A4, EVENTOUT | - | - |
| 15 | PF5 | I/O | FT_hvp | CN12.36, CN8.16(D50) | LPTIM3_CH1, OCTOSPIM_P2_NCLK, MDF1_CKI0, FMC_A5, EVENTOUT | - | - |
| 16 | VSS | S | - | - | - | - | - |
| 17 | VDD | S | - | CN11.5 | - | - | - |
| 18 | PF6 | I/O | FT_h | CN11.9 | TIM5_ETR, TIM5_CH1, DCMI_D12/PSSI_D12, OCTOSPIM_P2_NCS, OCTOSPIM_P1_IO3, SAI1_SD_B, EVENTOUT | - | - |
| 19 | PF7 | I/O | FT_h | CN11.11, CN9.26(D62) | TIM5_CH2, FDCAN1_RX, OCTOSPIM_P1_IO2, SAI1_MCLK_B, EVENTOUT | - | - |
| 20 | PF8 | I/O | FT_h | CN11.54, CN9.24(D61) | TIM5_CH3, PSSI_D14, FDCAN1_TX, OCTOSPIM_P1_IO0, SAI1_SCK_B, EVENTOUT | - | - |
| 21 | PF9 | I/O | FT_h | CN11.56, CN9.28(D63) | TIM5_CH4, PSSI_D15, OCTOSPIM_P1_IO1, SAI1_FS_B, TIM15_CH1, EVENTOUT | - | - |
| 22 | PF10 | I/O | FT_hv | CN12.42 | OCTOSPIM_P1_CLK, PSSI_D15, MDF1_CCK1, DCMI_D11/PSSI_D11, DSI_TE, SAI1_D3, TIM15_CH2, EVENTOUT | - | - |
| 23 | PH0-OSC_IN (PH0) | I/O | FT | CN11.29 | EVENTOUT | OSC_IN | - |
| 24 | PH1-OSC_OUT (PH1) | I/O | FT | CN11.31 | EVENTOUT | OSC_OUT | - |
| 25 | NRST | I/O | RST | CN11.14 | - | - | - |
| 26 | PC0 | I/O | FT_fha | CN11.38, CN9.11(A5) | LPTIM1_IN1, OCTOSPIM_P1_IO7, I2C3_SCL(boot), SPI2_RDY, MDF1_SDI4, USART6_CTS, LPUART1_RX, SDMMC1_D5, SAI2_FS_A, LPTIM2_IN1, EVENTOUT | ADC1_IN1, ADC2_IN1, ADC4_IN1 | - |
| 27 | PC1 | I/O | FT_fhav | CN11.36, CN9.9(A4) | TRACED0, LPTIM1_CH1, SPI2_MOSI, I2C3_SDA(boot), MDF1_CKI4, USART6_CK, LPUART1_TX, OCTOSPIM_P1_IO4, SDMMC2_CK, SAI1_SD_A, EVENTOUT | ADC1_IN2, ADC2_IN2, ADC4_IN2 | - |
| 28 | PC2 | I/O | FT_ha | CN11.35, CN10.9(A7) | LPTIM1_IN2, SPI2_MISO, MDF1_CCK1, USART6_RX, OCTOSPIM_P1_IO5, LPGPIO1_P5, EVENTOUT | ADC1_IN3, ADC2_IN3, ADC4_IN3 | - |
| 29 | PC3 | I/O | FT_ha | CN11.37, CN9.5(A2) | LPTIM1_ETR, LPTIM3_CH1, SAI1_D1, SPI2_MOSI, USART6_TX, OCTOSPIM_P1_IO6, SAI1_SD_A, LPTIM2_ETR, EVENTOUT | ADC1_IN4, ADC2_IN4, ADC4_IN4 | - |
| 30 | VSSA | S | - | - | - | - | - |
| 31 | VREF+ | S | - | - | - | VREFBUF_OUT | - |
| 32 | VDDA | S | - | - | - | - | - |
| 33 | PA0 | I/O | FT_hat | CN11.28, CN10.29(D32) | TIM2_CH1, TIM5_CH1, TIM8_ETR, SPI3_RDY, USART2_CTS, UART4_TX, OCTOSPIM_P2_NCS, SDMMC2_CMD, AUDIOCLK, TIM2_ETR, EVENTOUT | OPAMP1_VINP, ADC1_IN5, ADC2_IN5, WKUP1, TAMP_IN2/TAMP_OUT1 | - |
| 34 | PA1 | I/O | FT_hat | CN11.30, CN10.11(A8) | LPTIM1_CH2, TIM2_CH2, TIM5_CH2, I2C1_SMBA, SPI1_SCK, USART2_RTS_DE, UART4_RX, OCTOSPIM_P1_DQS, LPGPIO1_P0, TIM15_CH1N, EVENTOUT | OPAMP1_VINM, ADC1_IN6, ADC2_IN6, WKUP3, TAMP_IN5/TAMP_OUT4 | - |
| 35 | PA2 | I/O | FT_ha | CN12.35, CN9.3(A1), CN10.13(D26) | TIM2_CH3, TIM5_CH3, SPI1_RDY, USART2_TX(boot), LPUART1_TX, OCTOSPIM_P1_NCS, UCPD1_FRSTX1, TIM15_CH1, EVENTOUT | COMP1_INP3, ADC1_IN7, ADC2_IN7, WKUP4/LSCO | - |
| 36 | PA3 | I/O | TT_hav | CN12.37, CN9.1(A0) | TIM2_CH4, TIM5_CH4, SAI1_CK1, USART2_RX(boot), LPUART1_RX, OCTOSPIM_P1_CLK, LPGPIO1_P1, SAI1_MCLK_A, TIM15_CH2, EVENTOUT | OPAMP1_VOUT, ADC1_IN8, ADC2_IN8, WKUP5 | - |
| 37 | VSS | S | - | - | - | - | - |
| 38 | VDD | S | - | CN11.5 | - | - | - |
| 39 | PA4 | I/O | TT_ha | CN11.32, CN7.9(D20), CN7.17(D24) | OCTOSPIM_P1_NCS, SPI1_NSS(boot), SPI3_NSS, USART2_CK, DCMI_HSYNC/PSSI_D_E, SAI1_FS_B, LPTIM2_CH1, EVENTOUT | ADC1_IN9, ADC2_IN9, ADC4_IN9, DAC1_OUT1, WKUP2 | - |
| 40 | PA5 | I/O | TT_a | CN12.11, CN7.10(D13) | CSLEEP, TIM2_CH1, TIM2_ETR, TIM8_CH1N, PSSI_D14, SPI1_SCK(boot), USART3_RX, LPTIM2_ETR, EVENTOUT | ADC1_IN10, ADC2_IN10, ADC4_IN10, DAC1_OUT2, WKUP6 | - |
| 41 | PA6 | I/O | FT_ha | CN12.13, CN7.12(D12) | CDSTOP, TIM1_BKIN, TIM3_CH1, TIM8_BKIN, DCMI_PIXCLK/PSSI_P_DCK, SPI1_MISO(boot), USART3_CTS, LPUART1_CTS, OCTOSPIM_P1_IO3, LPGPIO1_P2, TIM16_CH1, EVENTOUT | OPAMP2_VINP, ADC1_IN11, ADC2_IN11, ADC4_IN11, WKUP7 | - |
| 42 | PA7 | I/O | FT_fha | CN12.15, CN7.14(D11) | SRDSTOP, TIM1_CH1N, TIM3_CH2, TIM8_CH1N, I2C3_SCL, SPI1_MOSI(boot), USART3_TX, OCTOSPIM_P1_IO2, LPTIM2_CH2, TIM17_CH1, EVENTOUT | OPAMP2_VINM, ADC1_IN12, ADC2_IN12, ADC4_IN20, WKUP8 | - |
| 43 | PB0 | I/O | TT_ha | CN11.34, CN9.7(A3), CN10.21(D29) | TIM1_CH2N, TIM3_CH3, TIM8_CH2N, LPTIM3_CH1, SPI1_NSS, USART3_CK, OCTOSPIM_P1_IO1, LPGPIO1_P9, COMP1_OUT, AUDIOCLK, EVENTOUT | OPAMP2_VOUT, ADC1_IN15, ADC2_IN15, ADC4_IN18 | - |
| 44 | PB1 | I/O | FT_ha | CN12.24, CN10.7(A6) | TIM1_CH3N, TIM3_CH4, TIM8_CH3N, LPTIM3_CH2, MDF1_SDI0, USART3_RTS_DE, LPUART1_RTS_DE, OCTOSPIM_P1_IO0, LPGPIO1_P3, LPTIM2_IN1, EVENTOUT | COMP1_INM1, ADC1_IN16, ADC2_IN16, ADC4_IN19, WKUP4 | - |
| 45 | PB2 | I/O | FT_hat | CN12.22, CN9.13(D72) | LPTIM1_CH1, TIM8_CH4N, I2C3_SMBA, SPI1_RDY, MDF1_CKI0, LCD_B1, OCTOSPIM_P1_DQS, UCPD1_FRSTX1, EVENTOUT | COMP1_INP2, ADC1_IN17, ADC2_IN17, WKUP1, RTC_OUT2 | - |
| 46 | PF11 | I/O | FT_hv | CN12.62 | OCTOSPIM_P1_NCLK, LCD_DE, DCMI_D12/PSSI_D12, DSI_TE, LPTIM4_IN1, EVENTOUT | - | - |
| 47 | PF12 | I/O | FT_h | CN12.59, CN7.20(D8) | OCTOSPIM_P2_DQS, LCD_B0, FMC_A6, LPTIM4_ETR, EVENTOUT | - | - |
| 48 | VSS | S | - | - | - | - | - |
| 49 | VDD | S | - | CN11.5 | - | - | - |
| 50 | PF13 | I/O | FT_h | CN12.57, CN10.2(D7) | I2C4_SMBA, LCD_B1, UCPD1_FRSTX2, FMC_A7, LPTIM4_OUT, EVENTOUT | - | - |
| 51 | PF14 | I/O | FT_fha | CN12.50, CN10.8(D4) | I2C4_SCL, LCD_G0, TSC_G8_IO1, FMC_A8, EVENTOUT | ADC4_IN5 | - |
| 52 | PF15 | I/O | FT_fha | CN12.60, CN10.12(D2) | I2C4_SDA, LCD_G1, TSC_G8_IO2, FMC_A9, EVENTOUT | ADC4_IN6 | - |
| 53 | PG0 | I/O | FT_ha | CN11.59, CN9.29(D65) | OCTOSPIM_P2_IO4, TSC_G8_IO3, FMC_A10, EVENTOUT | ADC4_IN7 | - |
| 54 | PG1 | I/O | FT_ha | CN11.58, CN9.30(D64) | OCTOSPIM_P2_IO5, TSC_G8_IO4, FMC_A11, EVENTOUT | ADC4_IN8 | - |
| 55 | PE7 | I/O | FT_h | CN12.44, CN10.20(D41) | TIM1_ETR, MDF1_SDI2, LCD_B6, FMC_D4, SAI1_SD_B, EVENTOUT | WKUP6 | - |
| 56 | PE8 | I/O | FT_h | CN12.40, CN10.18(D42) | TIM1_CH1N, MDF1_CKI2, LCD_B7, FMC_D5, SAI1_SCK_B, EVENTOUT | WKUP7 | - |
| 57 | PE9 | I/O | FT_hv | CN12.52, CN10.4(D6) | TIM1_CH1, ADF1_CCK0, MDF1_CCK0, LCD_G2, OCTOSPIM_P1_NCLK, FMC_D6, SAI1_FS_B, EVENTOUT | - | - |
| 58 | VSS | S | - | - | - | - | - |
| 59 | VDD | S | - | CN11.5 | - | - | - |
| 60 | PE10 | I/O | FT_hav | CN12.47, CN10.24(D40) | TIM1_CH2N, ADF1_SDI0, MDF1_SDI4, LCD_G3, TSC_G5_IO1, OCTOSPIM_P1_CLK, FMC_D7, SAI1_MCLK_B, EVENTOUT | - | - |
| 61 | PE11 | I/O | FT_ha | CN12.56, CN10.6(D5) | TIM1_CH2, SPI1_RDY, MDF1_CKI4, LCD_G4, TSC_G5_IO2, OCTOSPIM_P1_NCS, FMC_D8, EVENTOUT | - | - |
| 62 | PE12 | I/O | FT_ha | CN12.49, CN10.23(D30), CN10.26(D39) | TIM1_CH3N, SPI1_NSS, MDF1_SDI5, LCD_G5, TSC_G5_IO3, OCTOSPIM_P1_IO0, FMC_D9, EVENTOUT | - | - |
| 63 | PE13 | I/O | FT_ha | CN12.55, CN10.10(D3) | TIM1_CH3, SPI1_SCK, MDF1_CKI5, LCD_G6, TSC_G5_IO4, OCTOSPIM_P1_IO1, FMC_D10, EVENTOUT | - | - |
| 64 | PE14 | I/O | FT_h | CN12.51, CN10.25(D31), CN10.28(D38) | TIM1_CH4, TIM1_BKIN2, SPI1_MISO, LCD_G7, OCTOSPIM_P1_IO2, FMC_D11, EVENTOUT | - | - |
| 65 | PE15 | I/O | FT_h | CN12.53, CN10.19(D28), CN10.30(D37) | TIM1_BKIN, TIM1_CH4N, SPI1_MOSI, LCD_R2, OCTOSPIM_P1_IO3, FMC_D12, EVENTOUT | - | - |
| 66 | PB10 | I/O | FT_fhv | CN12.25, CN10.15(D27), CN10.32(D36) | TIM2_CH3, LPTIM3_CH1, I2C4_SCL, I2C2_SCL(boot), SPI2_SCK, USART3_TX, LPUART1_RX, TSC_SYNC, OCTOSPIM_P1_CLK, LPGPIO1_P4, COMP1_OUT, SAI1_SCK_A, EVENTOUT | WKUP8 | - |
| 67 | PB11 | I/O | FT_fh | CN12.18, CN10.34(D35) | TIM2_CH4, I2C4_SDA, I2C2_SDA(boot), SPI2_RDY, USART3_RX, LPUART1_TX, OCTOSPIM_P1_NCS, COMP2_OUT, EVENTOUT | - | - |
| 68 | VLXSMPS | S | - | - | - | - | - |
| 69 | VDDSMPS | S | - | - | - | - | - |
| 70 | VSSSMPS | S | - | - | - | - | - |
| 71 | VDD11 | S | - | - | - | - | - |
| 72 | VSS | S | - | - | - | - | - |
| 73 | VDD | S | - | CN11.5 | - | - | - |
| 74 | PB13 | I/O | FT_fa | CN12.30, CN7.5(D18) | TIM1_CH1N, LPTIM3_IN1, I2C2_SCL, SPI2_SCK(boot), MDF1_CKI1, USART3_CTS, LPUART1_CTS, TSC_G1_IO2, SAI2_SCK_A, TIM15_CH1N, EVENTOUT | - | - |
| 75 | PB14 | I/O | FT_fda | CN12.28 | TIM1_CH2N, LPTIM3_ETR, TIM8_CH2N, I2C2_SDA, SPI2_MISO(boot), MDF1_SDI2, USART3_RTS_DE, TSC_G1_IO3, SDMMC2_D0, SAI2_MCLK_A, TIM15_CH1, EVENTOUT | UCPD1_DBCC2 | - |
| 76 | PB15 | I/O | FT_c | CN12.26 | RTC_REFIN, TIM1_CH3N, LPTIM2_IN2, TIM8_CH3N, SPI2_MOSI(boot), MDF1_CKI2, FMC_NBL1, SDMMC2_D1, SAI2_SD_A, TIM15_CH2, EVENTOUT | UCPD1_CC2, WKUP7 | (4) |
| 77 | PD8 | I/O | FT_h | CN12.10 | USART3_TX, LCD_R3, DCMI_HSYNC/PSSI_D_E, FMC_D13, EVENTOUT | - | - |
| 78 | PD9 | I/O | FT_h | CN11.69 | LPTIM2_IN2, USART3_RX, LCD_R4, DCMI_PIXCLK/PSSI_P_DCK, FMC_D14, SAI2_MCLK_A, LPTIM3_IN1, EVENTOUT | - | - |
| 79 | PD10 | I/O | FT_ha | CN12.65 | LPTIM2_CH2, I2C5_SMBA, USART3_CK, LCD_R5, TSC_G6_IO1, FMC_D15, SAI2_SCK_A, LPTIM3_ETR, EVENTOUT | - | - |
| 80 | PD11 | I/O | FT_ha | CN12.45, CN7.3(D17) | I2C4_SMBA, USART3_CTS, LCD_R6, TSC_G6_IO2, FMC_CLE/FMC_A16, SAI2_SD_A, LPTIM2_ETR, EVENTOUT | ADC4_IN15 | - |
| 81 | PD12 | I/O | FT_fha | CN12.43, CN7.7(D19) | TIM4_CH1, I2C4_SCL, USART3_RTS_DE, LCD_R7, TSC_G6_IO3, FMC_ALE/FMC_A17, SAI2_FS_A, LPTIM2_IN1, EVENTOUT | ADC4_IN16 | - |
| 82 | PD13 | I/O | FT_fha | CN12.41 | TIM4_CH2, I2C4_SDA, USART6_CTS, LCD_VSYNC, TSC_G6_IO4, LPGPIO1_P6, FMC_A18, LPTIM4_IN1, LPTIM2_CH1, EVENTOUT | ADC4_IN17 | - |
| 83 | VSS | S | - | - | - | - | - |
| 84 | VDD | S | - | CN11.5 | - | - | - |
| 85 | PD14 | I/O | FT_h | CN12.46, CN7.16(D10) | TIM4_CH3, USART6_CK, LCD_B2, FMC_D0, LPTIM3_CH1, EVENTOUT | - | - |
| 86 | PD15 | I/O | FT_h | CN12.48, CN7.18(D9) | TIM4_CH4, USART6_RTS_DE, LCD_B3, FMC_D1, LPTIM3_CH2, EVENTOUT | - | - |
| 87 | PG2 | I/O | FT_hs | CN11.42 | SPI1_SCK, FMC_A12, SAI2_SCK_B, EVENTOUT | - | - |
| 88 | PG3 | I/O | FT_hs | CN11.44 | SPI1_MISO, FMC_A13, SAI2_FS_B, EVENTOUT | - | - |
| 89 | PG4 | I/O | FT_hs | CN12.69 | SPI1_MOSI, FMC_A14, SAI2_MCLK_B, EVENTOUT | - | - |
| 90 | PG5 | I/O | FT_hs | CN12.68 | SPI1_NSS, LPUART1_CTS, DSI_TE, FMC_A15, SAI2_SD_B, EVENTOUT | - | - |
| 91 | PG6 | I/O | FT_hs | CN12.70 | OCTOSPIM_P1_DQS, I2C3_SMBA, SPI1_RDY, LCD_R1, LPUART1_RTS_DE, UCPD1_FRSTX1, EVENTOUT | - | - |
| 92 | PG7 | I/O | FT_fhs | CN12.67, CN10.14(D1) | SAI1_CK1, I2C3_SCL, OCTOSPIM_P2_DQS, MDF1_CCK0, LPUART1_TX, UCPD1_FRSTX2, FMC_INT, SAI1_MCLK_A, EVENTOUT | - | - |
| 93 | PG8 | I/O | FT_fs | CN12.66, CN10.16(D0) | I2C3_SDA, LPUART1_RX, EVENTOUT | - | - |
| 94 | VSS | S | - | - | - | - | - |
| 95 | VDDIO2 | S | - | - | - | - | - |
| 96 | PC6 | I/O | FT_a | CN12.4, CN7.1(D16) | CSLEEP, TIM3_CH1, TIM8_CH1, MDF1_CKI3, LCD_R0, SDMMC1_D0DIR, TSC_G4_IO1, DCMI_D0/PSSI_D0, SDMMC2_D6, SDMMC1_D6, SAI2_MCLK_A, EVENTOUT | - | - |
| 97 | PC7 | I/O | FT_a | CN12.19 | CDSTOP, TIM3_CH2, TIM8_CH2, MDF1_SDI3, LCD_R1, SDMMC1_D123DIR, TSC_G4_IO2, DCMI_D1/PSSI_D1, SDMMC2_D7, SDMMC1_D7, SAI2_MCLK_B, LPTIM2_CH2, EVENTOUT | - | - |
| 98 | PC8 | I/O | FT_a | CN12.2, CN8.2(D43) | SRDSTOP, TIM3_CH3, TIM8_CH3, USART6_RX, TSC_G4_IO3, DCMI_D2/PSSI_D2, SDMMC1_D0, LPTIM3_CH1, EVENTOUT | - | - |
| 99 | PC9 | I/O | FT_a | CN12.1, CN8.4(D44) | TRACED0, TIM8_BKIN2, TIM3_CH4, TIM8_CH4, DCMI_D3/PSSI_D3, USART6_TX, TSC_G4_IO4, SDMMC1_D1, LPTIM3_CH2, EVENTOUT | - | - |
| 100 | PA8 | I/O | FT_hv | CN12.23, CN10.31(D33) | MCO, TIM1_CH1, SAI1_CK2, SPI1_RDY, USART1_CK, OTG_HS_SOF, TRACECLK, SAI1_SCK_A, LPTIM2_CH1, EVENTOUT | - | - |
| 101 | PA9 | I/O | FT_u | CN12.21 | TIM1_CH2, SPI2_SCK, DCMI_D0/PSSI_D0, USART1_TX(boot), SAI1_FS_A, TIM15_BKIN, EVENTOUT | OTG_HS_VBUS | - |
| 102 | PA10 | I/O | FT_u | CN12.33 | CRS_SYNC, TIM1_CH3, LPTIM2_IN2, SAI1_D1, DCMI_D1/PSSI_D1, USART1_RX(boot), OTG_HS_ID, SAI1_SD_A, TIM17_BKIN, EVENTOUT | - | - |
| 103 | PA11 | I/O | TT | CN12.14 | TIM1_CH4, TIM1_BKIN2, SPI1_MISO, USART1_CTS, FDCAN1_RX, EVENTOUT | OTG_HS_DM(boot) | (5) |
| 104 | PA12 | I/O | TT | CN12.12 | TIM1_ETR, SPI1_MOSI, OCTOSPIM_P2_NCS, USART1_RTS_DE, FDCAN1_TX, EVENTOUT | OTG_HS_DP(boot) | (5) |
| 105 | PA13 (JTMS/SWDIO) | I/O | FT | CN11.13 | JTMS/SWDIO, IR_OUT, SAI1_SD_B, EVENTOUT | - | (6) |
| 106 | VDDUSB | S | - | - | - | - | - |
| 107 | VSS | S | - | - | - | - | - |
| 108 | VDD | S | - | CN11.5 | - | - | - |
| 109 | PA14 (JTCK/SWCLK) | I/O | FT | CN11.15 | JTCK/SWCLK, LPTIM1_CH1, I2C1_SMBA, I2C4_SMBA, OTG_HS_SOF, SAI1_FS_B, EVENTOUT | - | (6) |
| 110 | PA15 (JTDI) | I/O | FT_c | CN11.17 | JTDI, TIM2_CH1, TIM2_ETR, USART2_RX, SPI1_NSS, SPI3_NSS, USART3_RTS_DE, UART4_RTS_DE, SAI2_FS_B, EVENTOUT | UCPD1_CC1 | (4) (6) |
| 111 | PC10 | I/O | FT_a | CN11.1, CN8.6(D45) | TRACED1, LPTIM3_ETR, ADF1_CCK1, SPI3_SCK, USART3_TX(boot), UART4_TX, TSC_G3_IO2, DCMI_D8/PSSI_D8, LPGPIO1_P8, SDMMC1_D2, SAI2_SCK_B, EVENTOUT | - | - |
| 112 | PC11 | I/O | FT_ha | CN11.2, CN8.8(D46) | LPTIM3_IN1, ADF1_SDI0, DCMI_D2/PSSI_D2, OCTOSPIM_P1_NCS, SPI3_MISO, USART3_RX(boot), UART4_RX, TSC_G3_IO3, DCMI_D4/PSSI_D4, UCPD1_FRSTX2, SDMMC1_D3, SAI2_MCLK_B, EVENTOUT | - | - |
| 113 | PC12 | I/O | FT_hav | CN11.3, CN8.10(D47) | TRACED3, SPI3_MOSI, USART3_CK, UART5_TX, TSC_G3_IO4, DCMI_D9/PSSI_D9, LPGPIO1_P10, SDMMC1_CK, SAI2_SD_B, EVENTOUT | - | - |
| 114 | PD0 | I/O | FT_fh | CN11.57, CN9.25(D67) | I2C6_SDA, TIM8_CH4N, I2C5_SDA, SPI2_NSS, LCD_B4, FDCAN1_RX, FMC_D2, EVENTOUT | - | - |
| 115 | PD1 | I/O | FT_fh | CN11.55, CN9.27(D66) | I2C6_SCL, I2C5_SCL, SPI2_SCK, LCD_B5, FDCAN1_TX, FMC_D3, EVENTOUT | - | - |
| 116 | PD2 | I/O | FT | CN11.4, CN8.12(D48) | TRACED2, TIM3_ETR, I2C5_SMBA, USART3_RTS_DE, UART5_RX, TSC_SYNC, DCMI_D11/PSSI_D11, LPGPIO1_P7, SDMMC1_CMD, LPTIM4_ETR, EVENTOUT | - | - |
| 117 | PD3 | I/O | FT_hv | CN11.40, CN9.10(D55) | I2C6_SMBA, SPI2_SCK, DCMI_D5/PSSI_D5, SPI2_MISO, MDF1_SDI0, USART2_CTS, LCD_CLK, OCTOSPIM_P2_NCS, FMC_CLK, EVENTOUT | - | - |
| 118 | PD4 | I/O | FT_h | CN11.39, CN9.8(D54) | SPI2_MOSI, MDF1_CKI0, USART2_RTS_DE, OCTOSPIM_P1_IO4, FMC_NOE, EVENTOUT | - | - |
| 119 | PD5 | I/O | FT_h | CN11.41, CN9.6(D53) | SPI2_RDY, USART2_TX, OCTOSPIM_P1_IO5, FMC_NWE, EVENTOUT | - | - |
| 120 | VSS | S | - | - | - | - | - |
| 121 | VDD | S | - | CN11.5 | - | - | - |
| 122 | PD6 | I/O | FT_hv | CN11.43, CN9.4(D52) | SAI1_D1, DCMI_D10/PSSI_D10, SPI3_MOSI, MDF1_SDI1, USART2_RX, LCD_DE, OCTOSPIM_P1_IO6, SDMMC2_CK, FMC_NWAIT, SAI1_SD_A, EVENTOUT | - | - |
| 123 | PD7 | I/O | FT_h | CN11.45, CN9.2(D51) | MDF1_CKI1, USART2_CK, OCTOSPIM_P1_IO7, SDMMC2_CMD, FMC_NCE/FMC_NE1, LPTIM4_OUT, EVENTOUT | - | - |
| 124 | PG9 | I/O | FT_hs | CN11.63 | OCTOSPIM_P2_IO6, SPI3_SCK(boot), USART1_TX, FMC_NCE/FMC_NE2, SAI2_SCK_A, TIM15_CH1N, EVENTOUT | - | - |
| 125 | PG10 | I/O | FT_hs | CN11.66 | LPTIM1_IN1, OCTOSPIM_P2_IO7, SPI3_MISO(boot), USART1_RX, FMC_NE3, SAI2_FS_A, TIM15_CH1, EVENTOUT | - | - |
| 126 | PG12 | I/O | FT_hs | CN11.65 | LPTIM1_ETR, OCTOSPIM_P2_NCS, SPI3_NSS(boot), USART1_RTS_DE, FMC_NE4, SAI2_SD_A, EVENTOUT | - | - |
| 127 | PG13 | I/O | FT_fhs | CN11.68 | I2C1_SDA, SPI3_RDY, USART1_CK, LCD_R0, FMC_A24, EVENTOUT | - | - |
| 128 | PG14 | I/O | FT_fhs | CN12.61 | LPTIM1_CH2, I2C1_SCL, LCD_R1, FMC_A25, EVENTOUT | - | - |
| 129 | VSS | S | - | - | - | - | - |
| 130 | VDDIO2 | S | - | - | - | - | - |
| 131 | PG15 | I/O | FT_hs | CN11.64 | LPTIM1_CH1, I2C1_SMBA, OCTOSPIM_P2_DQS, DCMI_D13/PSSI_D13, EVENTOUT | - | - |
| 132 | PB3 (JTDO/TRACESWO) | I/O | FT_fa | CN12.31, CN7.15(D23) | JTDO/TRACESWO, TIM2_CH2, LPTIM1_CH1, ADF1_CCK0, I2C1_SDA, SPI1_SCK, SPI3_SCK, USART1_RTS_DE, CRS_SYNC, LPGPIO1_P11, SDMMC2_D2, SAI1_SCK_B, EVENTOUT | COMP2_INM2 | - |
| 133 | PB4 (NJTRST) | I/O | FT_fa | CN12.27, CN7.11(D21), CN7.19(D25) | NJTRST, LPTIM1_CH2, TIM3_CH1, ADF1_SDI0, I2C3_SDA, SPI1_MISO, SPI3_MISO, USART1_CTS, UART5_RTS_DE, TSC_G2_IO1, DCMI_D12/PSSI_D12, LPGPIO1_P12, SDMMC2_D3, SAI1_MCLK_B, TIM17_BKIN, EVENTOUT | COMP2_INP1 | (6) |
| 134 | PB5 | I/O | FT_havc | CN12.29, CN7.13(D22) | LPTIM1_IN1, TIM3_CH2, OCTOSPIM_P1_NCLK, I2C1_SMBA, SPI1_MOSI, SPI3_MOSI(boot), USART1_CK, UART5_CTS, TSC_G2_IO2, DCMI_D10/PSSI_D10, COMP2_OUT, SAI1_SD_B, TIM16_BKIN, EVENTOUT | UCPD1_DBCC1, WKUP6 | - |
| 135 | PB6 | I/O | FT_fa | CN12.17, CN9.15(D71) | LPTIM1_ETR, TIM4_CH1, TIM8_BKIN2, I2C1_SCL(boot), I2C4_SCL, MDF1_SDI5, USART1_TX, TSC_G2_IO3, DCMI_D5/PSSI_D5, SAI1_FS_B, TIM16_CH1N, EVENTOUT | COMP2_INP2, WKUP3 | - |
| 136 | PB7 | I/O | FT_fhav | CN11.21 | LPTIM1_IN2, TIM4_CH2, TIM8_BKIN, I2C1_SDA(boot), I2C4_SDA, MDF1_CKI5, USART1_RX, UART4_CTS, TSC_G2_IO4, DCMI_VSYNC/PSSI_R_DY, FMC_NL, TIM17_CH1N, EVENTOUT | COMP2_INM1, PVD_IN, WKUP4 | - |
| 137 | PH3-BOOT0 | I/O | FT | CN11.7 | EVENTOUT | - | - |
| 138 | PB8 | I/O | FT_f | CN12.3, CN7.2(D15) | TIM4_CH3, SAI1_CK1, I2C1_SCL, MDF1_CCK0, SPI3_RDY, LCD_B1, SDMMC1_CKIN, FDCAN1_RX(boot), DCMI_D6/PSSI_D6, SDMMC2_D4, SDMMC1_D4, SAI1_MCLK_A, TIM16_CH1, EVENTOUT | WKUP5 | - |
| 139 | PB9 | I/O | FT_f | CN12.5, CN7.4(D14) | IR_OUT, TIM4_CH4, SAI1_D2, I2C1_SDA, SPI2_NSS, SDMMC1_CDIR, FDCAN1_TX(boot), DCMI_D7/PSSI_D7, SDMMC2_D5, SDMMC1_D5, SAI1_FS_A, TIM17_CH1, EVENTOUT | - | - |
| 140 | PE0 | I/O | FT_h | CN12.64, CN10.33(D34) | TIM4_ETR, USART6_RX, LCD_HSYNC, DCMI_D2/PSSI_D2, LPGPIO1_P13, FMC_NBL0, TIM16_CH1, EVENTOUT | - | - |
| 141 | PE1 | I/O | FT_h | CN11.61 | USART6_TX, LCD_VSYNC, DCMI_D3/PSSI_D3, FMC_NBL1, TIM17_CH1, EVENTOUT | - | - |
| 142 | VDD11 | S | - | - | - | - | - |
| 143 | VSS | S | - | - | - | - | - |
| 144 | VDD | S | - | CN11.5 | - | - | - |

## 2. Interfaces summary (STM32U5A5ZJT6Q)

Peripheral counts below are for the **ZJT (LQFP144, with SMPS)** column of the
datasheet's device-summary table; "5A5" subfamily = OTG_HS + hardware crypto, **no**
graphics accelerator (that's the 5A9 subfamily only — no LTDC/DSI/GPU2D/GFXMMU here).

| Interface | Count | Notes |
|---|---|---|
| SPI | 3 | Up to ~50 Mbit/s each; SPI2/SPI3 usable in Stop 1/Stop 2 (SPI3 only) with autonomous DMA. |
| I2C | 6 | Fm+ (1 Mbit/s) capable on `_f` pins; I2C3 usable down to Stop 2. |
| USART | 4 | Full-duplex, autonomous in Stop 0/1 with DMA. |
| UART | 2 | Subset of USART features. |
| LPUART | 1 | Runs from LSE, works down to **Stop 3**. |
| SAI | 2 (A/B blocks ×2) | I2S/PCM-capable audio serial interfaces — the interface to use for an external digital audio codec (not present on this project's current VS1053b path, which is SPI-based). |
| FDCAN | 1 | CAN FD, up to 8 Mbit/s data phase. |
| OCTOSPI | 2 | Muxed mode when both used simultaneously; one is also usable as a plain Quad-SPI (no DQS) as wired to the Nucleo Zio OCTOSPI pins. |
| HSPI | No | Not present on this subfamily/package. |
| SDMMC | 2 | One routed to Zio CN8 (SDMMC1, D43–D48) for the on-board/plug-in SD card. |
| USB OTG_HS | Yes | **On-chip HS PHY** (no external ULPI PHY needed), device/host/OTG roles, BC1.2 charging-port *detection* (see §4). |
| UCPD | Yes | USB Type-C CC detection + USB-PD 3.0 messaging + dead-battery support + FRS (see §4). |
| DCMI / PSSI | Yes | Parallel camera/parallel-sync sensor interface, shares pins with several GPIOs above (`DCMI_Dx/PSSI_Dx`). |
| MDF / ADF | Yes | Multi-function / audio digital filter — hardware PDM-to-PCM decimation front end for digital MEMS microphones, autonomous in Stop 0/1/2. |
| ADC | 1×12-bit + 2×14-bit | 22 channels (SMPS variant); ADC4 (12-bit) is the only ADC that keeps running down to Stop 2. |
| DAC | 1 controller, 2×12-bit channels | General-purpose DAC, **not** an audio-grade DAC — see §4.4. |
| OPAMP | 2 | Low-power, PGA-capable general-purpose op-amps — **not** audio power amplifiers, see §4.4. |
| Comparator | 2 | Ultra-low-power analog comparators. |
| GPIO | 111 (with SMPS) | Nearly all are broken out on the Nucleo Morpho/Zio headers (see §1). |
| Wake-up pins | 23 | Usable from Stop/Standby/Shutdown. |
| Timers | 2× adv-control (16-bit), 4×32-bit + 3×16-bit gen-purpose, 2×16-bit basic, 4×16-bit low-power (LPTIM1-4), 2× SysTick, 2× watchdog (IWDG/WWDG) | LPTIM1/3/4 run down to Stop 2. |
| RTC + TAMP | Yes | LSE/LSI clocked, 8 tamper pins, runs in every mode including Shutdown. |
| Crypto/security | RNG, AES+SAES, PKA, HASH (SHA-256), OTFDEC, TrustZone (Cortex-M33) | Full hardware crypto/RDP/TrustZone stack. |
| CORDIC / FMAC | Yes | Trig/DSP math accelerators, useful for on-device audio DSP (EQ, resampling) without a dedicated DSP chip. |

## 3. Low-power modes

From DS13543 Table 9 (modes overview) and §3.9 (VBAT operation), scoped to the SMPS
variant used on this board.

| Mode | Core/Flash | SRAM | What keeps running | Wake-up sources |
|---|---|---|---|---|
| **Run** (Range 1–4) | CPU on | on | Everything except (Range 4 only) DSI/OTG_HS/UCPD | — |
| **Sleep** (Range 1–4) | CPU stopped, peripherals on | on | Same peripheral set as matching Run range | Any interrupt/event |
| **Stop 0** | off | full retention | BOR/PVD/PVM, RTC/TAMP, IWDG, TEMP, ADC4, DAC1 (2ch), COMP1/2, OPAMP1/2, all USART/LPUART, SPI1-3, I2C1-6, LPTIM1-4, MDF1/ADF1, GPDMA1/LPDMA1 — all autonomous with DMA | Reset, all I/Os, + all of the above |
| **Stop 1** | off, main regulator off (LPR) | full retention | Same peripheral list as Stop 0 | Same as Stop 0 |
| **Stop 2** | off, LPR | full retention | BOR/PVD/PVM, RTC/TAMP, IWDG, TEMP, ADC4, DAC1, COMP1/2, OPAMP1/2, **LPUART1 only** (no other USART), **SPI3 only**, **I2C3 only**, LPTIM1/3/4, ADF1, LPDMA1 | Same list |
| **Stop 3** | off, LPR | full retention | BOR, RTC/TAMP, IWDG, DAC1 (2 *static* channels), OPAMP1/2 only — no comms peripherals | Reset, 24 WKUP pins, BOR/RTC/TAMP/IWDG |
| **Standby** | off | SRAM2 optionally retained (8/56/64 KB via LPR), else lost | BOR, RTC/TAMP, IWDG only; I/Os float or pull-up/down (no state retained) | Reset, 24 WKUP pins, BOR/RTC/TAMP/IWDG |
| **Shutdown** | off | lost (backup domain regs survive while VDD present) | RTC/TAMP only; **no BOR**, VBAT switch-over unsupported | Reset, WKUP pins, RTC event, tamper |

Practical read for an always-listening/battery audio device:
- **Stop 2** is the natural "idle, ready to resume fast" state: RTC/tamper/wake-up +
  LPUART1 (e.g. a UI/BT-module UART) + ADF1 (PDM mic front end, for wake-word/voice
  detection without waking the CPU) + I2C3 (a fuel-gauge/PMIC poll) all keep running
  autonomously via DMA.
- **Stop 3**/**Standby** are for "off but resumable via button/RTC" — no serial
  peripherals stay alive, only WKUP pins and RTC.
- **Shutdown** is the true "shipping mode" — lowest current, backup-domain registers
  only, no BOR (so don't rely on brown-out protection while in it).
- The **VBAT pin's internal trickle-charge circuit is only for a coin-cell/supercap
  backing the RTC/tamper/backup-SRAM domain** when the board is unpowered (§3.9.5 of the
  datasheet) — it is unrelated to, and cannot substitute for, charging the main system
  Li-Po cell (see §4.1).

## 4. System integration Q&A

Context: this project's current schematics already use a **MCP73831-2-MC** Li-Ion/Li-Po
charger (`hardware/openpod/power.kicad_sch`) with a ~900 mAh 3.7 V cell, an
**FSC-BT1035** Bluetooth audio module (`hardware/openpod/bluetooth.kicad_sch`, Feasycom
module built around a CSR/Qualcomm BT SoC), and a **VS1053b** for MP3/FLAC decode + DAC +
line-out (`software/src/sound/`, `hardware/openpod/audio.kicad_sch`). The STM32U5A5 is a
general-purpose MCU with no wireless radio and no battery-charging front end, so it
changes none of that: it would replace the current MCU only, not these companion chips.

### 4.1 Battery management — does it eliminate the MCP73831?

**No.** The STM32U5A5 has:
- A **VBAT pin trickle-charge circuit** (§3.9.5) — but this only charges a small
  coin-cell/super-cap backing the RTC/TAMP/backup-SRAM domain when the board is
  unpowered. It is not sized or intended for a system Li-Po cell.
- No CC/CV Li-Ion/Li-Po charge-management circuitry, no thermal regulation, no
  charge-status output — none of what MCP73831 provides.
- **UCPD** (USB-C PD controller) can *negotiate* how much current/voltage a USB-C source
  will allow (up to 5 A / 20 V under PD 3.0), and **OTG_HS** can *detect* charging-port
  type per BC1.2 — but both only tell you what's available on VBUS; neither one actually
  regulates that power into a charge profile for a battery cell.
- No fuel-gauge/coulomb-counter peripheral either — battery level monitoring would still
  need either an external fuel gauge (e.g. MAX17048-class) or a simple ADC-based
  resistor-divider voltage read on `VBAT`/a dedicated ADC pin (the chip's 22-channel ADC
  easily covers this).

**Conclusion**: keep the MCP73831 (or a Type‑C‑aware equivalent if you want to use
UCPD's negotiated current to program the charger's ISET). The STM32U5A5 does not replace
it.

### 4.2 Bluetooth — audio codecs (SBC/AAC/aptX), A2DP/SPP, can it be a BT *source*?

**Not applicable to this chip.** The STM32U5 series (per DS13543) has **no RF/Bluetooth
radio at all** — that's the STM32WB/WBA family, not U5. Bluetooth profiles (A2DP, SPP,
AVRCP, HFP) and their audio codecs (SBC mandatory, AAC/aptX optional) all live on the
**FSC-BT1035** module already in this design; the STM32U5A5 would only talk to it over
UART/SPI/I2S (its 4 USART + SAI give plenty of options), not implement any BT stack
itself.

Whether the design **can act as a BT audio source** (push audio *to* headphones/speakers,
i.e. A2DP source role, vs. sink/receiver role) is entirely a property of the FSC-BT1035's
chipset and firmware, not the host MCU — check Feasycom's datasheet/AT-command set for
that module specifically for source-mode support; it's independent of this MCU swap.

### 4.3 Audio decoding — MP3/FLAC etc.

**No hardware audio-decoder block on the STM32U5A5.** MP3/FLAC/etc. decoding is either:
- done by the existing **VS1053b** (current design — hardware MP3 decode + FLAC via
  loaded plugin, per `software/src/sound/FLAC_README.md`), or
- done in **software** on the Cortex-M33 (160 MHz, single-precision FPU, CORDIC/FMAC DSP
  accelerators) using a software decoder (e.g. libmad/minimp3, libFLAC) if you wanted to
  drop the VS1053b and decode on-MCU into the DAC/SAI path instead.

Swapping to on-MCU software decoding is a real option this chip's headroom makes
plausible (160 MHz M33 vs. the VS1053b's dedicated DSP core), but it's a firmware
rewrite, not something the datasheet "gives you for free."

### 4.4 DAC and amplifiers — resolution, THD/SNR, driving line-out

The STM32U5A5's analog blocks are **general-purpose**, not audio-grade:

| Block | Spec | Audio-relevance |
|---|---|---|
| DAC (×2 ch) | 12-bit, settling time ~1.7–3 µs, buffered output, PSRR ‑28 to ‑80 dB | No published THD/SNR (datasheet doesn't characterize it as an audio DAC — DAC accuracy is specified in DNL/INL/offset only). 12-bit ≈ 74 dB theoretical SNR at best, well below what the VS1053b or a dedicated audio DAC (typically 16–24 bit, 90+ dB SNR) delivers. |
| OPAMP (×2) | Rail-to-rail, PGA gain ×2/×4/×8/×16, **max drive current 500 µA** (100 µA low-power mode) | Far too little drive current for headphones/line-out (headphone amps need tens of mA); these are meant for sensor conditioning, not audio output. Datasheet explicitly notes the DAC's own output buffer exists "to drive external loads directly **without an external operational amplifier**" for light loads (RL ≥ 5 kΩ) — still not a speaker/headphone driver. |

**Conclusion**: the STM32U5A5's DAC/op-amps cannot replace the VS1053b's audio-grade DAC
and line-out drive, nor would they meet typical THD/SNR targets for a music player. Audio
output should stay on VS1053b (current design) or move to an external audio DAC/codec
over **SAI** (I2S) if VS1053b is dropped, with a separate headphone/line amplifier IC
(e.g. a small Class-AB or Class-D driver) for line-out — the STM32 has none built in.

### 4.5 USB management — do we need the MCP73831?

Same answer as §4.1, restated in USB terms: the STM32U5A5's **OTG_HS** (device/host/OTG,
on-chip HS PHY, BC1.2 port-type detection) and **UCPD** (USB-C CC detection, PD 3.0
negotiation, dead-battery support, FRS) together give you a complete USB-C
*port-management and data* front end — enumerate as a mass-storage/audio device, detect
a charger vs. a data host, negotiate a higher input current from a PD charger. What they
do **not** do is turn that negotiated VBUS power into a regulated Li-Po charge current —
that final stage is still the MCP73831's job. If you keep USB-C on the new design, UCPD
is worth using (dead-battery support alone is useful — see datasheet footnote on
PA15/PB15 pull-downs), but it sits *upstream of*, not *instead of*, the charger IC.
