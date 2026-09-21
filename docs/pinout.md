---
layout: PinoutLayout
hero:
  name: Nucleo-F446RE pinout
  tagline: Interactive STM32F446RE (LQFP64) pinout, sourced from the firmware — hover a pin, filter by function or peripheral, and export the mapping as code or markdown.
  actions:
    - text: Full pin table
      link: /pinout-table
      theme: alt
---

Interactive pinout of the **STM32F446RE** (LQFP64) as wired on the
[Nucleo-F446RE](https://www.st.com/en/evaluation-tools/nucleo-f446re.html) board.
The default pin mapping is extracted from the firmware sources
(`software/src/pinout.h`, `software/src/rendering/ILI9341_driver.h` and the
`TwoWire Wire3(PB4, PA8)` setup in `software/src/main.cpp`), so it always shows
what the code actually drives.

Switch between the **Nucleo board** (Arduino + Morpho headers), the **LQFP64
chip** package, and a searchable **pin table**. Hover a pin, a peripheral, or a
legend swatch to trace it; click to pin it open in the inspector on the right,
which also shows quick-assign and unassign controls. The **Design checks**
panel below it flags shared pins, JTAG/SWD reuse, and signals wired to a pin
that doesn't support them.

The mapping is editable in the workbench underneath — as a spreadsheet-style
table or as raw JSON — and exports to **C++**, **Markdown** or **JSON**. Edits
are kept in your browser's local storage; "Reset to firmware" restores the
built-in defaults.

<PinoutStudio />

## Pin mapping format

The mapping is a JSON array of peripherals, each with a list of pins. Every pin
has a `pin` (STM32 name like `PA5`, or a package number) and a `role`; it may
also carry a `macro` (the C `#define` name, used by the code export) and the
peripheral a `bus`:

```json
[
  {
    "peripheral": "VS1053",
    "bus": "SPI1",
    "pins": [
      { "pin": "PA11", "role": "MP3CS (chip select)", "macro": "MP3CS" },
      { "pin": "PA5", "role": "SPI1_SCK" }
    ]
  }
]
```

Switch the workbench above to its **JSON** tab, paste a mapping in this shape
and hit **Apply** — every view, the peripheral filters, the design checks and
the other exports all update. The built-in default mapping lives in
`docs/.vitepress/theme/components/pinmap/mapping.json`.

## Quick (flat) usage

For one-off highlighting without peripherals, the `Pinout` component also
accepts a `usage` prop in **JSON**, **CSV** or **Markdown** (auto-detected):

```md
<Pinout usage='[{"pin":"PA5","label":"SPI1_SCK"},{"pin":"PB8","label":"TFT_DC"}]' />
```

```csv
pin,function
PA5,SPI1_SCK
PB8,TFT_DC
```

```md
| Pin | Function |
| --- | --- |
| PA5 | SPI1_SCK |
| PB8 | TFT_DC |
```

## Full pin table

The complete 64-pin table — pin number, type, default function, alternate
functions, additional functions and Nucleo mapping — is generated from the same
data the component renders:

- [STM32F446RE LQFP64 pin table](./pinout-table.md)

<details>
<summary>Show the full table inline</summary>

| # | Pin | Type | Default function | Alternate functions | Additional | Arduino | Morpho |
|---|---|---|---|---|---|---|---|
| **1** | `VBAT` | Power | Backup power supply | — | — | — | CN7-33 |
| **2** | `PC13` | I/O · FT | GPIO | EVENTOUT | TAMP_1/WKUP1 | — | CN7-23 |
| **3** | `PC14-OSC32_IN` | I/O · FT | GPIO | EVENTOUT | OSC32_IN | — | CN7-25 |
| **4** | `PC15-OSC32_OUT` | I/O · FT | GPIO | EVENTOUT | OSC32_OUT | — | CN7-27 |
| **5** | `PH0-OSC_IN` | I/O · FT | GPIO | EVENTOUT | OSC_IN | — | CN7-29 |
| **6** | `PH1-OSC_OUT` | I/O · FT | GPIO | EVENTOUT | OSC_OUT | — | CN7-31 |
| **7** | `NRST` | Reset | System reset (weak pull-up) | — | — | RESET | CN7-14 |
| **8** | `PC0` | I/O · FT | GPIO | SAI1_MCLK_B, OTG_HS_ULPI_STP, FMC_SDNWE, EVENTOUT | ADC123_IN10 | A5 | CN7-38 |
| **9** | `PC1` | I/O · FT | GPIO | SPI3_MOSI/I2S3_SD, SAI1_SD_A, SPI2_MOSI/I2S2_SD, EVENTOUT | ADC123_IN11 | A4 | CN7-36 |
| **10** | `PC2` | I/O · FT | GPIO | SPI2_MISO, OTG_HS_ULPI_DIR, FMC_SDNE0, EVENTOUT | ADC123_IN12 | — | CN7-35 |
| **11** | `PC3` | I/O · FT | GPIO | SPI2_MOSI/I2S2_SD, OTG_HS_ULPI_NXT, FMC_SDCKE0, EVENTOUT | ADC123_IN13 | — | CN7-37 |
| **12** | `VSSA` | Power | Analog ground | — | — | — | — |
| **13** | `VDDA` | Power | Analog supply | — | — | — | — |
| **14** | `PA0-WKUP` | I/O · FT | GPIO | TIM2_CH1/TIM2_ETR, TIM5_CH1, TIM8_ETR, USART2_CTS, UART4_TX, EVENTOUT | ADC123_IN0, WKUP0/TAMP_2 | A0 | CN7-28 |
| **15** | `PA1` | I/O · FT | GPIO | TIM2_CH2, TIM5_CH2, USART2_RTS, UART4_RX, QUADSPI_BK1_IO3, SAI2_MCLK_B, EVENTOUT | ADC123_IN1 | A1 | CN7-30 |
| **16** | `PA2` | I/O · FT | GPIO | TIM2_CH3, TIM5_CH3, TIM9_CH1, USART2_TX, SAI2_SCK_B, EVENTOUT | ADC123_IN2 | D1 | CN10-35 |
| **17** | `PA3` | I/O · FT | GPIO | TIM2_CH4, TIM5_CH4, TIM9_CH2, SAI1_FS_A, USART2_RX, OTG_HS_ULPI_D0, EVENTOUT | ADC123_IN3 | D0 | CN10-37 |
| **18** | `VSS` | Power | Ground | — | — | — | — |
| **19** | `VDD` | Power | Digital supply | — | — | — | — |
| **20** | `PA4` | I/O · TTa | GPIO | SPI1_NSS/I2S1_WS, SPI3_NSS/I2S3_WS, USART2_CK, OTG_HS_SOF, DCMI_HSYNC, EVENTOUT | ADC12_IN4, DAC_OUT1 | A2 | CN7-32 |
| **21** | `PA5` | I/O · TTa | GPIO | TIM2_CH1/TIM2_ETR, TIM8_CH1N, SPI1_SCK/I2S1_CK, OTG_HS_ULPI_CK, EVENTOUT | ADC12_IN5, DAC_OUT2 | D13 | CN10-11 |
| **22** | `PA6` | I/O · FT | GPIO | TIM1_BKIN, TIM3_CH1, TIM8_BKIN, SPI1_MISO, I2S2_MCK, TIM13_CH1, DCMI_PIXCLK, EVENTOUT | ADC12_IN6 | D12 | CN10-13 |
| **23** | `PA7` | I/O · FT | GPIO | TIM1_CH1N, TIM3_CH2, TIM8_CH1N, SPI1_MOSI/I2S1_SD, TIM14_CH1, FMC_SDNWE, EVENTOUT | ADC12_IN7 | D11 | CN10-15 |
| **24** | `PC4` | I/O · FT | GPIO | I2S1_MCK, SPDIFRX_IN2, FMC_SDNE0, EVENTOUT | ADC12_IN14 | — | CN10-34 |
| **25** | `PC5` | I/O · FT | GPIO | USART3_RX, SPDIFRX_IN3, FMC_SDCKE0, EVENTOUT | ADC12_IN15 | — | CN10-6 |
| **26** | `PB0` | I/O · FT | GPIO | TIM1_CH2N, TIM3_CH3, TIM8_CH2N, SPI3_MOSI/I2S3_SD, UART4_CTS, OTG_HS_ULPI_D1, SDIO_D1, EVENTOUT | ADC12_IN8 | A3 | CN7-34 |
| **27** | `PB1` | I/O · FT | GPIO | TIM1_CH3N, TIM3_CH4, TIM8_CH3N, OTG_HS_ULPI_D2, SDIO_D2, EVENTOUT | ADC12_IN9 | — | CN10-24 |
| **28** | `PB2-BOOT1` | I/O · FT | BOOT1 | TIM2_CH4, SAI1_SD_A, SPI3_MOSI/I2S3_SD, QUADSPI_CLK, OTG_HS_ULPI_D4, SDIO_CK, EVENTOUT | — | — | CN10-22 |
| **29** | `PB10` | I/O · FT | GPIO | TIM2_CH3, I2C2_SCL, SPI2_SCK/I2S2_CK, SAI1_SCK_A, USART3_TX, OTG_HS_ULPI_D3, EVENTOUT | — | D6 | CN10-25 |
| **30** | `VCAP_1` | Power | Regulator capacitor | — | — | — | — |
| **31** | `VSS` | Power | Ground | — | — | — | — |
| **32** | `VDD` | Power | Digital supply | — | — | — | — |
| **33** | `PB12` | I/O · FT | GPIO | TIM1_BKIN, I2C2_SMBA, SPI2_NSS/I2S2_WS, SAI1_SCK_B, USART3_CK, CAN2_RX, OTG_HS_ULPI_D5, OTG_HS_ID, EVENTOUT | — | — | CN10-16 |
| **34** | `PB13` | I/O · FT | GPIO | TIM1_CH1N, SPI2_SCK/I2S2_CK, USART3_CTS, CAN2_TX, OTG_HS_ULPI_D6, EVENTOUT | OTG_HS_VBUS | — | CN10-30 |
| **35** | `PB14` | I/O · FT | GPIO | TIM1_CH2N, TIM8_CH2N, SPI2_MISO, USART3_RTS, TIM12_CH1, OTG_HS_DM, EVENTOUT | — | — | CN10-28 |
| **36** | `PB15` | I/O · FT | GPIO | RTC_REFIN, TIM1_CH3N, TIM8_CH3N, SPI2_MOSI/I2S2_SD, TIM12_CH2, OTG_HS_DP, EVENTOUT | — | — | CN10-26 |
| **37** | `PC6` | I/O · FTf | GPIO | TIM3_CH1, TIM8_CH1, FMPI2C1_SCL, I2S2_MCK, USART6_TX, SDIO_D6, DCMI_D0, EVENTOUT | — | — | CN10-4 |
| **38** | `PC7` | I/O · FTf | GPIO | TIM3_CH2, TIM8_CH2, FMPI2C1_SDA, SPI2_SCK/I2S2_CK, I2S3_MCK, SPDIFRX_IN1, USART6_RX, SDIO_D7, DCMI_D1, EVENTOUT | — | D9 | CN10-19 |
| **39** | `PC8` | I/O · FT | GPIO | TRACED0, TIM3_CH3, TIM8_CH3, UART5_RTS, USART6_CK, SDIO_D0, DCMI_D2, EVENTOUT | — | — | CN10-2 |
| **40** | `PC9` | I/O · FT | GPIO | MCO2, TIM3_CH4, TIM8_CH4, I2C3_SDA, I2S_CKIN, UART5_CTS, QUADSPI_BK1_IO0, SDIO_D1, DCMI_D3, EVENTOUT | — | — | CN10-1 |
| **41** | `PA8` | I/O · FT | GPIO | MCO1, TIM1_CH1, I2C3_SCL, USART1_CK, OTG_FS_SOF, EVENTOUT | — | D7 | CN10-23 |
| **42** | `PA9` | I/O · FT | GPIO | TIM1_CH2, I2C3_SMBA, SPI2_SCK/I2S2_CK, SAI1_SD_B, USART1_TX, DCMI_D0, EVENTOUT | OTG_FS_VBUS | D8 | CN10-21 |
| **43** | `PA10` | I/O · FT | GPIO | TIM1_CH3, USART1_RX, OTG_FS_ID, DCMI_D1, EVENTOUT | — | D2 | CN10-33 |
| **44** | `PA11` | I/O · FT | GPIO | TIM1_CH4, USART1_CTS, CAN1_RX, OTG_FS_DM, EVENTOUT | — | — | CN10-14 |
| **45** | `PA12` | I/O · FT | GPIO | TIM1_ETR, USART1_RTS, SAI2_FS_B, CAN1_TX, OTG_FS_DP, EVENTOUT | — | — | CN10-12 |
| **46** | `PA13` | I/O · FT | SWD data (JTMS-SWDIO) | JTMS-SWDIO, EVENTOUT | — | — | CN7-13 |
| **47** | `VSS` | Power | Ground | — | — | — | — |
| **48** | `VDD` | Power | Digital supply | — | — | — | — |
| **49** | `PA14` | I/O · FT | SWD clock (JTCK-SWCLK) | JTCK-SWCLK, EVENTOUT | — | — | CN7-15 |
| **50** | `PA15` | I/O · FT | JTDI | JTDI, TIM2_CH1/TIM2_ETR, HDMI_CEC, SPI1_NSS/I2S1_WS, SPI3_NSS/I2S3_WS, UART4_RTS, EVENTOUT | — | — | CN7-17 |
| **51** | `PC10` | I/O · FT | GPIO | SPI3_SCK/I2S3_CK, USART3_TX, UART4_TX, QUADSPI_BK1_IO1, SDIO_D2, DCMI_D8, EVENTOUT | — | — | CN7-1 |
| **52** | `PC11` | I/O · FT | GPIO | SPI3_MISO, USART3_RX, UART4_RX, QUADSPI_BK2_NCS, SDIO_D3, DCMI_D4, EVENTOUT | — | — | CN7-2 |
| **53** | `PC12` | I/O · FT | GPIO | I2C2_SDA, SPI3_MOSI/I2S3_SD, USART3_CK, UART5_TX, SDIO_CK, DCMI_D9, EVENTOUT | — | — | CN7-3 |
| **54** | `PD2` | I/O · FT | GPIO | TIM3_ETR, UART5_RX, SDIO_CMD, DCMI_D11, EVENTOUT | — | — | CN7-4 |
| **55** | `PB3` | I/O · FT | JTDO/TRACESWO | JTDO/TRACESWO, TIM2_CH2, I2C2_SDA, SPI1_SCK/I2S1_CK, SPI3_SCK/I2S3_CK, EVENTOUT | — | D3 | CN10-31 |
| **56** | `PB4` | I/O · FT | NJTRST | NJTRST, TIM3_CH1, I2C3_SDA, SPI1_MISO, SPI3_MISO, SPI2_NSS/I2S2_WS, EVENTOUT | — | D5 | CN10-27 |
| **57** | `PB5` | I/O · FT | GPIO | TIM3_CH2, I2C1_SMBA, SPI1_MOSI/I2S1_SD, SPI3_MOSI/I2S3_SD, CAN2_RX, OTG_HS_ULPI_D7, FMC_SDCKE1, DCMI_D10, EVENTOUT | — | D4 | CN10-29 |
| **58** | `PB6` | I/O · FT | GPIO | TIM4_CH1, HDMI_CEC, I2C1_SCL, USART1_TX, CAN2_TX, QUADSPI_BK1_NCS, FMC_SDNE1, DCMI_D5, EVENTOUT | — | D10 | CN10-17 |
| **59** | `PB7` | I/O · FT | GPIO | TIM4_CH2, I2C1_SDA, USART1_RX, SPDIFRX_IN0, FMC_NL, DCMI_VSYNC, EVENTOUT | — | — | CN7-21 |
| **60** | `BOOT0` | Boot | Boot configuration | — | VPP | — | CN7-7 |
| **61** | `PB8` | I/O · FT | GPIO | TIM2_CH1/TIM2_ETR, TIM4_CH3, TIM10_CH1, I2C1_SCL, CAN1_RX, SDIO_D4, DCMI_D6, EVENTOUT | — | D15, A5 (alt) | CN10-3, CN7-38 |
| **62** | `PB9` | I/O · FT | GPIO | TIM2_CH2, TIM4_CH4, TIM11_CH1, I2C1_SDA, SPI2_NSS/I2S2_WS, SAI1_FS_B, CAN1_TX, SDIO_D5, DCMI_D7, EVENTOUT | — | D14, A4 (alt) | CN10-5, CN7-36 |
| **63** | `VSS` | Power | Ground | — | — | — | — |
| **64** | `VDD` | Power | Digital supply | — | — | — | — |

</details>

## Sources

- [STM32F446xC/E datasheet (DS10693)](https://www.st.com/resource/en/datasheet/stm32f446re.pdf) — local copy in `datasheets/stm32f446re.pdf`
- [Nucleo user manual UM1724](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf) — local copy in `datasheets/nucleo.pdf`
- Firmware pin mapping: `software/src/pinout.h`, `software/src/rendering/ILI9341_driver.h`, `software/src/main.cpp`
