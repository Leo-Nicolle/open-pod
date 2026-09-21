export interface ConnectorPin {
  pos: number;
  label: string;
  chip?: string;
  altChip?: string;
  func?: string;
  kind?: "nc" | "power" | "ref";
}

export interface Connector {
  id: string;
  title: string;
  /** how many physical columns of pins this header exposes (Morpho has 2) */
  columns: ConnectorPin[][];
}

const ard = (pos: number, label: string, chip?: string, func?: string, altChip?: string): ConnectorPin => ({
  pos,
  label,
  chip,
  func,
  altChip,
});

const net = (pos: number, label: string, kind: ConnectorPin["kind"]): ConnectorPin => ({
  pos,
  label,
  kind,
});

const morpho = (pos: number, label: string, chip?: string, altChip?: string): ConnectorPin => ({
  pos,
  label,
  chip,
  altChip,
});

/**
 * Nucleo-F446RE connectors, as documented in UM1724 (Rev 16):
 *  - Table 19: ARDUINO connectors
 *  - Table 29: ST morpho connector
 * `chip` is the STM32 pin name; `undefined` means a board net (power / NC / ref).
 */
export const CONNECTORS: Connector[] = [
  {
    id: "CN6",
    title: "CN6 · Arduino power",
    columns: [
      [
        net(1, "NC", "nc"),
        net(2, "IOREF", "ref"),
        ard(3, "RESET", "NRST", "Reset"),
        net(4, "+3.3V", "power"),
        net(5, "+5V", "power"),
        net(6, "GND", "power"),
        net(7, "GND", "power"),
        net(8, "VIN", "power"),
      ],
    ],
  },
  {
    id: "CN8",
    title: "CN8 · Arduino analog",
    columns: [
      [
        ard(1, "A0", "PA0", "ADC123_IN0"),
        ard(2, "A1", "PA1", "ADC123_IN1"),
        ard(3, "A2", "PA4", "ADC12_IN4"),
        ard(4, "A3", "PB0", "ADC12_IN8"),
        ard(5, "A4", "PC1", "ADC123_IN11", "PB9"),
        ard(6, "A5", "PC0", "ADC123_IN10", "PB8"),
      ],
    ],
  },
  {
    id: "CN5",
    title: "CN5 · Arduino digital",
    columns: [
      [
        ard(10, "D15", "PB8", "I2C1_SCL"),
        ard(9, "D14", "PB9", "I2C1_SDA"),
        net(8, "AREF", "ref"),
        net(7, "GND", "power"),
        ard(6, "D13", "PA5", "SPI1_SCK"),
        ard(5, "D12", "PA6", "SPI1_MISO"),
        ard(4, "D11", "PA7", "TIM14_CH1 / SPI1_MOSI"),
        ard(3, "D10", "PB6", "TIM4_CH1 / SPI1_CS"),
        ard(2, "D9", "PC7", "TIM8_CH2"),
        ard(1, "D8", "PA9"),
      ],
    ],
  },
  {
    id: "CN9",
    title: "CN9 · Arduino digital",
    columns: [
      [
        ard(8, "D7", "PA8"),
        ard(7, "D6", "PB10", "TIM2_CH3"),
        ard(6, "D5", "PB4", "TIM3_CH1"),
        ard(5, "D4", "PB5"),
        ard(4, "D3", "PB3", "TIM2_CH2"),
        ard(3, "D2", "PA10"),
        ard(2, "D1", "PA2", "USART2_TX"),
        ard(1, "D0", "PA3", "USART2_RX"),
      ],
    ],
  },
  {
    id: "CN7",
    title: "CN7 · Morpho (left)",
    columns: [
      [
        morpho(1, "PC10", "PC10"),
        morpho(3, "PC12", "PC12"),
        net(5, "VDD", "power"),
        morpho(7, "BOOT0", "BOOT0"),
        net(9, "—", "nc"),
        net(11, "—", "nc"),
        morpho(13, "PA13", "PA13"),
        morpho(15, "PA14", "PA14"),
        morpho(17, "PA15", "PA15"),
        net(19, "GND", "power"),
        morpho(21, "PB7", "PB7"),
        morpho(23, "PC13", "PC13"),
        morpho(25, "PC14", "PC14"),
        morpho(27, "PC15", "PC15"),
        morpho(29, "PH0", "PH0"),
        morpho(31, "PH1", "PH1"),
        morpho(33, "VBAT", "VBAT"),
        morpho(35, "PC2", "PC2"),
        morpho(37, "PC3", "PC3"),
      ],
      [
        morpho(2, "PC11", "PC11"),
        morpho(4, "PD2", "PD2"),
        net(6, "E5V", "power"),
        net(8, "GND", "power"),
        net(10, "—", "nc"),
        net(12, "IOREF", "ref"),
        ard(14, "RESET", "NRST", "Reset"),
        net(16, "+3.3V", "power"),
        net(18, "+5V", "power"),
        net(20, "GND", "power"),
        net(22, "GND", "power"),
        net(24, "VIN", "power"),
        net(26, "—", "nc"),
        morpho(28, "PA0", "PA0"),
        morpho(30, "PA1", "PA1"),
        morpho(32, "PA4", "PA4"),
        morpho(34, "PB0", "PB0"),
        morpho(36, "PC1", "PC1", "PB9"),
        morpho(38, "PC0", "PC0", "PB8"),
      ],
    ],
  },
  {
    id: "CN10",
    title: "CN10 · Morpho (right)",
    columns: [
      [
        morpho(1, "PC9", "PC9"),
        morpho(3, "PB8", "PB8"),
        morpho(5, "PB9", "PB9"),
        net(7, "AVDD", "power"),
        net(9, "GND", "power"),
        morpho(11, "PA5", "PA5"),
        morpho(13, "PA6", "PA6"),
        morpho(15, "PA7", "PA7"),
        morpho(17, "PB6", "PB6"),
        morpho(19, "PC7", "PC7"),
        morpho(21, "PA9", "PA9"),
        morpho(23, "PA8", "PA8"),
        morpho(25, "PB10", "PB10"),
        morpho(27, "PB4", "PB4"),
        morpho(29, "PB5", "PB5"),
        morpho(31, "PB3", "PB3"),
        morpho(33, "PA10", "PA10"),
        morpho(35, "PA2", "PA2"),
        morpho(37, "PA3", "PA3"),
      ],
      [
        morpho(2, "PC8", "PC8"),
        morpho(4, "PC6", "PC6"),
        morpho(6, "PC5", "PC5"),
        net(8, "U5V", "power"),
        net(10, "—", "nc"),
        morpho(12, "PA12", "PA12"),
        morpho(14, "PA11", "PA11"),
        morpho(16, "PB12", "PB12"),
        net(18, "—", "nc"),
        net(20, "GND", "power"),
        morpho(22, "PB2", "PB2"),
        morpho(24, "PB1", "PB1"),
        morpho(26, "PB15", "PB15"),
        morpho(28, "PB14", "PB14"),
        morpho(30, "PB13", "PB13"),
        net(32, "AGND", "power"),
        morpho(34, "PC4", "PC4"),
        net(36, "—", "nc"),
        net(38, "—", "nc"),
      ],
    ],
  },
];
