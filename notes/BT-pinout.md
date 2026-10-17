# FSC-BT1035 pinout and quick wiring

Pin names and descriptions are from `datasheets/FSC-BT1035_Datasheet_EN.pdf`
(table 3-2). The physical layout matches the footprint
`hardware/opentone/opentone.pretty/fsc-bt1035.kicad_mod`.

## Physical layout

Top view (shield side up), antenna at the bottom:

```
              31 30 29 28 27 26 25 24 23 22
            +-------------------------------+
         32 |                               | 21
         33 |                               | 20
         34 |                               | 19
         35 |                               | 18
         36 |                               | 17
         37 |                               | 16
         38 |                               | 15
         39 |                               | 14
         40 |                               | 13
         41 |                               | 12
         42 |                               | 11
         43 |                               | 10
         44 |                               | 9
         45 |                               | 8
         46 |                               | 7
         47 |                               | 6
         48 |                               | 5
         49 |                               | 4
         50 |                               | 3
         51 |                               | 2
         52 |                               | 1
            |                               |
            |            ANTENNA            |
            +-------------------------------+
```

- 1.0 mm pitch. Module 13.0 x 26.9 mm.
- Right edge: pins 1-21, bottom to top. Top edge: 22-31, right to left. Left
  edge: 32-52, top to bottom.
- **Dead-bug (pads facing up), left and right swap:** pin 1 is then on the
  left, pin 52 on the right.
- Keep the antenna end clear of metal and wires.

## Quick wiring (module on wires, current Nucleo)

| Module pin | Name | Connect to | Notes |
|---|---|---|---|
| 33 | VBAT_IN | Nucleo 3V3 | 10 µF + 100 nF across pins 32/33, on the module |
| 36 | VDD_IO | Nucleo 3V3 | I/O level = 3.3 V |
| 32 | GND | GND | |
| 1, 22, 50, 52 | GND | GND | at least one more ground, for the radio |
| 34 | SYS_CTRL | PA15 | drive high > 20 ms after power-up to boot. No internal pull: never leave floating |
| 14 | BT_RX | PB6 (USART1 TX) | AT commands, 115200 8N1 |
| 13 | BT_TX | PB7 (USART1 RX) | |
| 4 | PCM_CLK | VS1053 GPIO6 (SCLK, LQFP pin 11) | I2S bit clock |
| 7 | PCM_SYNC | VS1053 GPIO4 (LROUT, LQFP pin 36) | I2S word clock |
| 5 | PCM_IN | VS1053 GPIO7 (SDATA, LQFP pin 12) | I2S data into the module |
| 8 | RESET | optional: GPIO, open-drain | internal pull-up, can stay unconnected |
| 17 | AIO0/LED0 | optional: LED + 1 k to 3V3 | open-drain. Blinks while pairing (reference design) |

Power the module before driving UART or I2S, otherwise it gets powered through
its I/O pins.

### I2S: VS1053 to module

VS1053 pin numbers are for its LQFP-48 package (`datasheets/vs1053.pdf`, pin
table). Note the module pins are not in order.

| Signal | VS1053 pin | VS1053 LQFP pin | Module pin | Module signal |
|---|---|---|---|---|
| Word clock (48 kHz) | GPIO4 / I2S_LROUT | 36 | 7 | PCM_SYNC |
| Bit clock (1.536 MHz) | GPIO6 / I2S_SCLK | 11 | 4 | PCM_CLK |
| Data | GPIO7 / I2S_SDATA | 12 | 5 | PCM_IN |

Settings that match: the VS1053 as I2S master at 48 kHz (I2S_CONFIG = 0x4, MCLK
off), and the module as slave, 48 kHz, 16-bit I2S: `AT+AUXCFG=3` (I2S input)
and `AT+I2SCFG=3`.

## All pins

| Pin | Name | Type | Description |
|---|---|---|---|
| 1 | GND | Ground | |
| 2 | AIO4/LED4 | A, I/O | Analog/digital input or open-drain LED output |
| 3 | AIO5/LED5 | A, I/O | Analog/digital input or open-drain LED output |
| 4 | PCM_CLK/PIO16 | I/O | I2S/PCM bit clock |
| 5 | PCM_IN/PIO19 | I/O | I2S/PCM data in |
| 6 | PCM_OUT/PIO18 | I/O | PCM data out (I2S is input-only on this module) |
| 7 | PCM_SYNC/PIO17 | I/O | I2S word clock / PCM sync |
| 8 | RESET | I/O | Active-low reset, internal pull-up to VDD_IO. Can become a PIO after boot |
| 9 | PCM_MCLK_OUT/PIO15 | I/O | I2S master clock out |
| 10 | PIO6 | I/O | Debug/programming (TBR_MOSI) |
| 11 | PIO7 | I/O | Debug/programming (TBR_MISO) |
| 12 | PIO8 | I/O | Debug/programming (TBR_CLK) |
| 13 | BT_TX/PIO5 | I/O | UART TX |
| 14 | BT_RX/PIO4 | I/O | UART RX |
| 15 | BT_CTS/PIO3 | I/O | UART CTS |
| 16 | BT_RTS/PIO2 | I/O | UART RTS |
| 17 | AIO0/LED0 | A, I/O | Analog/digital input or open-drain LED output |
| 18 | AIO1/LED1 | A, I/O | Analog/digital input or open-drain LED output |
| 19 | AIO2/LED2 | A, I/O | Analog/digital input or open-drain LED output |
| 20 | PIO44 | I/O | |
| 21 | PIO41 | I/O | |
| 22 | GND | Ground | |
| 23 | PIO42 | I/O | |
| 24 | PIO43 | I/O | |
| 25 | PIO40 | I/O | |
| 26 | PIO45 | I/O | |
| 27 | PIO20 | I/O | Alt: PCM_DOUT[1] |
| 28 | PIO21 | I/O | |
| 29 | VCHG_SENSE | | Charger sense. Connect to VCHG when using the internal charger |
| 30 | NC | | |
| 31 | VDD_USB/3.3V_OUT | Power out | 3.3 V output, 50 mA max (2.8-3.0 V when running from VBAT) |
| 32 | GND | Ground | |
| 33 | VBAT_IN | Power | Supply, 3.0-4.2 V |
| 34 | SYS_CTRL | Input | Power-on signal: high > 20 ms boots the module. No pull, VBAT tolerant |
| 35 | 1.8V_OUT | Power out | 1.8 V output |
| 36 | VDD_IO | Power | I/O supply, 1.7-3.3 V |
| 37 | USB_DP | | USB full-speed D+ |
| 38 | USB_DN | | USB full-speed D- |
| 39 | VCHG | Power | USB VBUS / charger input (4.75-6.5 V). Keeps the module powered while present |
| 40 | MIC_RP | Analog | Mic 2 / line-in right, positive |
| 41 | MIC_RN | Analog | Mic 2 / line-in right, negative |
| 42 | NC | | |
| 43 | MIC_LP | Analog | Mic 1 / line-in left, positive |
| 44 | MIC_LN | Analog | Mic 1 / line-in left, negative |
| 45 | MIC_BIAS | Power out | Mic bias |
| 46 | NC | | |
| 47 | NC | | |
| 48 | SPK_N | Analog | Differential line out, negative |
| 49 | SPK_P | Analog | Differential line out, positive |
| 50 | GND | Ground | |
| 51 | RF_OUT | RF | External antenna (leave unconnected: the antenna is on the module) |
| 52 | GND | Ground | |
