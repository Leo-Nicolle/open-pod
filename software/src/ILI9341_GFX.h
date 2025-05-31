// ILI9341 16-bit parallel driver for STM32F4 (Arduino + PlatformIO)
// FIXED VERSION - Corrected data bus mapping and read operations
// Wrapped in ILI9341_GFX class - Adafruit_GFX compatible
#include <Arduino.h>
#include <Adafruit_GFX.h>

class ILI9341_GFX : public Adafruit_GFX {
public:
  // Pin definitions
  static const int TFT_DC = PC10;
  static const int TFT_WR = PC11;
  static const int TFT_RST = PC12;
  static const int TFT_RD = PD2;

  // Data bus mapping:
  // D0-D1 → PB0, PB1
  // D2 → PB10  
  // D3-D7 → PB12, PB13, PB14, PB15, PC0
  // D8-D15 → PC1, PC2, PC3, PC4, PC5, PC6, PC7, PC8

  void setupPins() {
    // Control pins
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_WR, OUTPUT);
    pinMode(TFT_RST, OUTPUT);
    pinMode(TFT_RD, OUTPUT);

    digitalWrite(TFT_WR, HIGH);
    digitalWrite(TFT_RST, HIGH);
    digitalWrite(TFT_RD, HIGH); // RD inactive

    // Enable GPIO clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN;

    // Set data pins to output mode
    // PB0, PB1, PB10, PB12-PB15
    GPIOB->MODER &= ~(0b11 << (0 * 2) | 0b11 << (1 * 2) |
                      0b11 << (10 * 2) | 0b11 << (12 * 2) |
                      0b11 << (13 * 2) | 0b11 << (14 * 2) | 0b11 << (15 * 2));
    GPIOB->MODER |=  (0b01 << (0 * 2) | 0b01 << (1 * 2) |
                      0b01 << (10 * 2) | 0b01 << (12 * 2) |
                      0b01 << (13 * 2) | 0b01 << (14 * 2) | 0b01 << (15 * 2));

    // PC0-PC8, PC10-PC12
    GPIOC->MODER &= ~(0b11 << (0 * 2) | 0b11 << (1 * 2) | 0b11 << (2 * 2) |
                      0b11 << (3 * 2) | 0b11 << (4 * 2) | 0b11 << (5 * 2) |
                      0b11 << (6 * 2) | 0b11 << (7 * 2) | 0b11 << (8 * 2) |
                      0b11 << (10 * 2) | 0b11 << (11 * 2) | 0b11 << (12 * 2));
    GPIOC->MODER |=  (0b01 << (0 * 2) | 0b01 << (1 * 2) | 0b01 << (2 * 2) |
                      0b01 << (3 * 2) | 0b01 << (4 * 2) | 0b01 << (5 * 2) |
                      0b01 << (6 * 2) | 0b01 << (7 * 2) | 0b01 << (8 * 2) |
                      0b01 << (10 * 2) | 0b01 << (11 * 2) | 0b01 << (12 * 2));
    
    // Set all data pins to push-pull, high speed
    GPIOB->OTYPER &= ~((1 << 0) | (1 << 1) | (1 << 10) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15));
    GPIOC->OTYPER &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8));
  }

  inline void pulseWR() {
    GPIOC->BSRR = (1 << (11 + 16));  // WR low
    __NOP(); // Small delay
    GPIOC->BSRR = (1 << 11);         // WR high
  }

  inline void write16(uint16_t data) {
    // Try alternative bit mapping based on your schematic
    // Your schematic: D0–D7 → PB0, PB1, PB10, PB12, PB13, PB14, PB15, PC0
    // Your schematic: D8–D15 → PC1, PC2, PC3, PC4, PC5, PC6, PC7, PC8
    
    uint32_t b_val = 0;
    uint32_t c_val = 0;
    
    // Map LOW byte (D0-D7) to mixed GPIOB/GPIOC pins
    b_val |= ((data >> 0) & 0x01) << 0;   // D0 → PB0
    b_val |= ((data >> 1) & 0x01) << 1;   // D1 → PB1  
    b_val |= ((data >> 2) & 0x01) << 10;  // D2 → PB10
    b_val |= ((data >> 3) & 0x01) << 12;  // D3 → PB12
    b_val |= ((data >> 4) & 0x01) << 13;  // D4 → PB13
    b_val |= ((data >> 5) & 0x01) << 14;  // D5 → PB14
    b_val |= ((data >> 6) & 0x01) << 15;  // D6 → PB15
    c_val |= ((data >> 7) & 0x01) << 0;   // D7 → PC0
    
    // Map HIGH byte (D8-D15) to GPIOC pins PC1-PC8
    c_val |= ((data >> 8) & 0x01) << 1;   // D8 → PC1
    c_val |= ((data >> 9) & 0x01) << 2;   // D9 → PC2
    c_val |= ((data >> 10) & 0x01) << 3;  // D10 → PC3
    c_val |= ((data >> 11) & 0x01) << 4;  // D11 → PC4
    c_val |= ((data >> 12) & 0x01) << 5;  // D12 → PC5
    c_val |= ((data >> 13) & 0x01) << 6;  // D13 → PC6
    c_val |= ((data >> 14) & 0x01) << 7;  // D14 → PC7
    c_val |= ((data >> 15) & 0x01) << 8;  // D15 → PC8
    
    // Apply the values
    uint32_t b_mask = (1 << 0) | (1 << 1) | (1 << 10) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 15);
    uint32_t c_mask = 0x1FF; // PC0-PC8
    
    GPIOB->ODR = (GPIOB->ODR & ~b_mask) | b_val;
    GPIOC->ODR = (GPIOC->ODR & ~c_mask) | c_val;
    
    pulseWR();
  }

  void setBusToInput() {
    // Set data pins to input mode
    GPIOB->MODER &= ~((0b11 << (0 * 2)) | (0b11 << (1 * 2)) |
                      (0b11 << (10 * 2)) | (0b11 << (12 * 2)) |
                      (0b11 << (13 * 2)) | (0b11 << (14 * 2)) |
                      (0b11 << (15 * 2)));

    GPIOC->MODER &= ~((0b11 << (0 * 2)) | (0b11 << (1 * 2)) |
                      (0b11 << (2 * 2)) | (0b11 << (3 * 2)) |
                      (0b11 << (4 * 2)) | (0b11 << (5 * 2)) |
                      (0b11 << (6 * 2)) | (0b11 << (7 * 2)) |
                      (0b11 << (8 * 2)));
  }

  void setBusToOutput() {
    // Set data pins back to output mode
    GPIOB->MODER |= (0b01 << (0 * 2)) | (0b01 << (1 * 2)) |
                    (0b01 << (10 * 2)) | (0b01 << (12 * 2)) |
                    (0b01 << (13 * 2)) | (0b01 << (14 * 2)) |
                    (0b01 << (15 * 2));

    GPIOC->MODER |= (0b01 << (0 * 2)) | (0b01 << (1 * 2)) |
                    (0b01 << (2 * 2)) | (0b01 << (3 * 2)) |
                    (0b01 << (4 * 2)) | (0b01 << (5 * 2)) |
                    (0b01 << (6 * 2)) | (0b01 << (7 * 2)) |
                    (0b01 << (8 * 2));
  }

  uint16_t readBus16() {
    uint16_t value = 0;
    uint32_t b = GPIOB->IDR;
    uint32_t c = GPIOC->IDR;

    // FIXED: Correct bit extraction
    value |= ((b >> 0) & 0x01) << 0;   // D0 ← PB0
    value |= ((b >> 1) & 0x01) << 1;   // D1 ← PB1
    value |= ((b >> 10) & 0x01) << 2;  // D2 ← PB10
    value |= ((b >> 12) & 0x01) << 3;  // D3 ← PB12
    value |= ((b >> 13) & 0x01) << 4;  // D4 ← PB13
    value |= ((b >> 14) & 0x01) << 5;  // D5 ← PB14
    value |= ((b >> 15) & 0x01) << 6;  // D6 ← PB15
    value |= ((c >> 0) & 0x01) << 7;   // D7 ← PC0
    value |= ((c >> 1) & 0x01) << 8;   // D8 ← PC1
    value |= ((c >> 2) & 0x01) << 9;   // D9 ← PC2
    value |= ((c >> 3) & 0x01) << 10;  // D10 ← PC3
    value |= ((c >> 4) & 0x01) << 11;  // D11 ← PC4
    value |= ((c >> 5) & 0x01) << 12;  // D12 ← PC5
    value |= ((c >> 6) & 0x01) << 13;  // D13 ← PC6
    value |= ((c >> 7) & 0x01) << 14;  // D14 ← PC7
    value |= ((c >> 8) & 0x01) << 15;  // D15 ← PC8

    return value;
  }

  uint16_t read16() {
    setBusToInput();
    
    // RD low → start read
    digitalWrite(TFT_RD, LOW);
    delayMicroseconds(2); // Increased delay for tACC
    
    uint16_t value = readBus16();
    
    // RD high → end read
    digitalWrite(TFT_RD, HIGH);
    
    setBusToOutput();
    return value;
  }

  void writeCommand(uint8_t cmd) {
    digitalWrite(TFT_DC, LOW);
    write16(cmd);
  }

  void writeData(uint8_t data) {
    digitalWrite(TFT_DC, HIGH);
    write16(data);
  }

  void writeData16(uint16_t data) {
    digitalWrite(TFT_DC, HIGH);
    write16(data);
  }

  void readRegisterBytes(uint8_t reg, uint8_t *buffer, uint8_t len) {
    writeCommand(reg);
    digitalWrite(TFT_DC, HIGH);  // Read data mode
    
    // First read is often dummy data
    read16();
    
    for (uint8_t i = 0; i < len; i++) {
      uint16_t val = read16();
      buffer[i] = val & 0xFF;
    }
  }

  uint16_t readRegister(uint8_t reg) {
    writeCommand(reg);
    digitalWrite(TFT_DC, HIGH);  // Data mode
    read16(); // Dummy read
    return read16();
  }

  void initILI9341() {
    digitalWrite(TFT_RST, LOW);
    delay(20);
    digitalWrite(TFT_RST, HIGH);
    delay(150);

    writeCommand(0x01); // Software reset
    delay(100);

    writeCommand(0x28); // Display OFF
    
    // Power Control A
    writeCommand(0xCB);
    writeData(0x39);
    writeData(0x2C);
    writeData(0x00);
    writeData(0x34);
    writeData(0x02);
    
    // Power Control B
    writeCommand(0xCF);
    writeData(0x00);
    writeData(0xC1);
    writeData(0x30);
    
    // Driver Timing Control A
    writeCommand(0xE8);
    writeData(0x85);
    writeData(0x00);
    writeData(0x78);
    
    // Driver Timing Control B
    writeCommand(0xEA);
    writeData(0x00);
    writeData(0x00);
    
    // Power on Sequence Control
    writeCommand(0xED);
    writeData(0x64);
    writeData(0x03);
    writeData(0x12);
    writeData(0x81);
    
    // Pump Ratio Control
    writeCommand(0xF7);
    writeData(0x20);
    
    // Power Control 1
    writeCommand(0xC0);
    writeData(0x23);
    
    // Power Control 2
    writeCommand(0xC1);
    writeData(0x10);
    
    // VCOM Control 1
    writeCommand(0xC5);
    writeData(0x3E);
    writeData(0x28);
    
    // VCOM Control 2
    writeCommand(0xC7);
    writeData(0x86);
    
    // Memory Access Control
    writeCommand(0x36);
    writeData(0x48);
    
    // Pixel Format Set
    writeCommand(0x3A);
    writeData(0x55); // 16-bit color
    
    // Frame Rate Control
    writeCommand(0xB1);
    writeData(0x00);
    writeData(0x18);
    
    // Display Function Control
    writeCommand(0xB6);
    writeData(0x08);
    writeData(0x82);
    writeData(0x27);
    
    // 3Gamma Function Disable
    writeCommand(0xF2);
    writeData(0x00);
    
    // Gamma Curve Selected
    writeCommand(0x26);
    writeData(0x01);
    
    // Set Gamma
    writeCommand(0xE0);
    writeData(0x0F);
    writeData(0x31);
    writeData(0x2B);
    writeData(0x0C);
    writeData(0x0E);
    writeData(0x08);
    writeData(0x4E);
    writeData(0xF1);
    writeData(0x37);
    writeData(0x07);
    writeData(0x10);
    writeData(0x03);
    writeData(0x0E);
    writeData(0x09);
    writeData(0x00);
    
    // Set Gamma
    writeCommand(0xE1);
    writeData(0x00);
    writeData(0x0E);
    writeData(0x14);
    writeData(0x03);
    writeData(0x11);
    writeData(0x07);
    writeData(0x31);
    writeData(0xC1);
    writeData(0x48);
    writeData(0x08);
    writeData(0x0F);
    writeData(0x0C);
    writeData(0x31);
    writeData(0x36);
    writeData(0x0F);
    
    // Sleep Out
    writeCommand(0x11);
    delay(120);
    
    // Display ON
    writeCommand(0x29);
    delay(20);
  }

  void printRegister(const char* name, uint8_t reg, uint8_t len) {
    uint8_t data[4] = {0};
    readRegisterBytes(reg, data, len);

    Serial.print(name);
    Serial.print(" (0x");
    Serial.print(reg, HEX);
    Serial.print("): ");

    for (uint8_t i = 0; i < len; i++) {
      Serial.print("0x");
      if (data[i] < 0x10) Serial.print("0");
      Serial.print(data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

  void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Column Address Set
    writeCommand(0x2A); 
    writeData16((x0 << 8) | (x0 >> 8)); // Send as 16-bit with byte swap
    writeData16((x1 << 8) | (x1 >> 8)); // Send as 16-bit with byte swap
    
    // Page Address Set  
    writeCommand(0x2B);
    writeData16((y0 << 8) | (y0 >> 8)); // Send as 16-bit with byte swap
    writeData16((y1 << 8) | (y1 >> 8)); // Send as 16-bit with byte swap
    
    writeCommand(0x2C); // Memory Write
  }

  void setWindowDebug(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    Serial.print("setWindow: x0="); Serial.print(x0);
    Serial.print(" y0="); Serial.print(y0);
    Serial.print(" x1="); Serial.print(x1);
    Serial.print(" y1="); Serial.println(y1);
    
    // Column Address Set
    Serial.println("Setting Column Address (0x2A)");
    writeCommand(0x2A); 
    
    // Try sending coordinates as separate high/low bytes instead of byte-swapped
    Serial.print("  X0: 0x"); Serial.print(x0 >> 8, HEX); Serial.print(" 0x"); Serial.println(x0 & 0xFF, HEX);
    writeData(x0 >> 8);   // High byte of x0
    writeData(x0 & 0xFF); // Low byte of x0
    Serial.print("  X1: 0x"); Serial.print(x1 >> 8, HEX); Serial.print(" 0x"); Serial.println(x1 & 0xFF, HEX);
    writeData(x1 >> 8);   // High byte of x1
    writeData(x1 & 0xFF); // Low byte of x1
    
    // Page Address Set  
    Serial.println("Setting Page Address (0x2B)");
    writeCommand(0x2B);
    Serial.print("  Y0: 0x"); Serial.print(y0 >> 8, HEX); Serial.print(" 0x"); Serial.println(y0 & 0xFF, HEX);
    writeData(y0 >> 8);   // High byte of y0
    writeData(y0 & 0xFF); // Low byte of y0
    Serial.print("  Y1: 0x"); Serial.print(y1 >> 8, HEX); Serial.print(" 0x"); Serial.println(y1 & 0xFF, HEX);
    writeData(y1 >> 8);   // High byte of y1
    writeData(y1 & 0xFF); // Low byte of y1
    
    Serial.println("Setting Memory Write (0x2C)");
    writeCommand(0x2C); // Memory Write
    
    // Let's also try an alternative approach for comparison
    Serial.println("Alternative: trying different coordinate format...");
  }

  // Alternative window setting method to test
  void setWindowAlt2(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Some displays expect coordinates in different format
    writeCommand(0x2A); // Column Address Set
    writeData16(x0);    // No byte swapping
    writeData16(x1);
    
    writeCommand(0x2B); // Page Address Set
    writeData16(y0);    // No byte swapping  
    writeData16(y1);
    
    writeCommand(0x2C); // Memory Write
  }

  // Test method to verify window setting
  // Debug and test methods
  void testDisplayDimensions() {
    Serial.println("=== Display Dimensions Test ===");
    
    fillScreen(0x0000); // Black background
    delay(500);
    
    // Test 1: Draw lines at expected boundaries
    Serial.println("Drawing boundary lines...");
    
    // Top edge (should be a horizontal line at top)
    setWindowDebug(0, 0, 319, 0);
    for (int i = 0; i < 320; i++) {
      writeData16(0xF800); // Red
    }
    
    // Bottom edge (should be a horizontal line at bottom)  
    setWindowDebug(0, 239, 319, 239);
    for (int i = 0; i < 320; i++) {
      writeData16(0xF800); // Red
    }
    
    // Left edge (should be a vertical line at left)
    setWindowDebug(0, 0, 0, 239);
    for (int i = 0; i < 240; i++) {
      writeData16(0x07E0); // Green
    }
    
    // Right edge (should be a vertical line at right)
    setWindowDebug(319, 0, 319, 239);
    for (int i = 0; i < 240; i++) {
      writeData16(0x07E0); // Green
    }
    
    delay(3000);
  }

  void testMADCTL() {
    Serial.println("=== MADCTL Test ===");
    
    // Try different MADCTL values to see which one works
    uint8_t madctl_values[] = {0x48, 0x28, 0x88, 0xE8, 0x08, 0x68, 0xA8, 0xC8};
    const char* names[] = {"0x48 (orig)", "0x28", "0x88", "0xE8", "0x08", "0x68", "0xA8", "0xC8"};
    
    for (int i = 0; i < 8; i++) {
      Serial.print("Testing MADCTL: "); Serial.println(names[i]);
      
      writeCommand(0x36); // MADCTL
      writeData(madctl_values[i]);
      delay(100);
      
      // Fill with a color and draw a test pattern
      fillScreen(0x0000); // Black
      
      // Draw a small rectangle in top-left to see orientation
      setWindowDebug(10, 10, 50, 30);
      for (int j = 0; j < 41 * 21; j++) {
        writeData16(0xF800); // Red rectangle
      }
      
      // Draw text to see if wrapping is fixed
      setCursor(10, 50);
      setTextColor(0x07E0); // Green
      setTextSize(1);
      print("MADCTL: "); println(names[i]);
      println("This is a test line to check wrapping behavior");
      testDisplayDimensions();
      
      delay(3000);
    }
  }


  void setWindowAlt(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Alternative windowing - try if first doesn't work
    writeCommand(0x2A); // Column Address Set
    writeData16(x0);
    writeData16(x1);
    
    writeCommand(0x2B); // Page Address Set
    writeData16(y0);
    writeData16(y1);
    
    writeCommand(0x2C); // Memory Write
  }

// public:
  // Constructor - Initialize Adafruit_GFX with display dimensions
  ILI9341_GFX() : Adafruit_GFX(320, 240) {
    // Constructor can be empty since setupPins() and initILI9341() will be called explicitly
  }

  // Required Adafruit_GFX virtual methods
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height)) return;
    
    setWindow(x, y, x, y);
    writeData16(color);
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    if ((x >= _width) || (y >= _height)) return;
    
    int16_t x2 = x + w - 1, y2 = y + h - 1;
    if ((x2 < 0) || (y2 < 0)) return;
    
    // Clip to screen boundaries
    if (x < 0) {
      w += x;
      x = 0;
    }
    if (y < 0) {
      h += y;
      y = 0;
    }
    if (x2 >= _width) {
      w = _width - x;
    }
    if (y2 >= _height) {
      h = _height - y;
    }
    
    // Debug output
    Serial.print("fillRect: x="); Serial.print(x);
    Serial.print(" y="); Serial.print(y);
    Serial.print(" w="); Serial.print(w);
    Serial.print(" h="); Serial.print(h);
    Serial.print(" x2="); Serial.print(x + w - 1);
    Serial.print(" y2="); Serial.println(y + h - 1);
    
    setWindowDebug(x, y, x + w - 1, y + h - 1);
    for (int32_t i = 0; i < (int32_t)w * h; i++) {
      writeData16(color);
    }
  }

  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    fillRect(x, y, 1, h, color);
  }

  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    fillRect(x, y, w, 1, color);
  }

  void fillScreen(uint16_t color) override {
    fillRect(0, 0, _width, _height, color);
  }

  void setRotation(uint8_t r) override {
    rotation = r & 3;
    switch (rotation) {
      case 0:
        _width = 320;
        _height = 240;
        writeCommand(0x36);
        writeData(0x48);
        break;
      case 1:
        _width = 240;
        _height = 320;
        writeCommand(0x36);
        writeData(0x28);
        break;
      case 2:
        _width = 320;
        _height = 240;
        writeCommand(0x36);
        writeData(0x88);
        break;
      case 3:
        _width = 240;
        _height = 320;
        writeCommand(0x36);
        writeData(0xE8);
        break;
    }
  }

  void invertDisplay(bool i) {
    writeCommand(i ? 0x21 : 0x20);
  }

  // Public methods
  void begin() {
    setupPins();
    initILI9341();
    setRotation(0); // Set default rotation
  }

  // Enhanced fillScreen that uses the optimized setWindow method
  // void fillScreen(uint16_t color) {
  //   setWindow(0, 0, _width - 1, _height - 1);
  //   for (uint32_t i = 0; i < (uint32_t)_width * _height; i++) {
  //     writeData16(color);
  //   }
  // }

  void detectController() {
    Serial.println("=== Controller Detection ===");
    
    // Try different ID registers
    printRegister("ID (0x04)", 0x04, 3);
    printRegister("ID (0xD3)", 0xD3, 4);  // Alternative ID register
    printRegister("ID (0xDA)", 0xDA, 1);  // Read ID1
    printRegister("ID (0xDB)", 0xDB, 1);  // Read ID2
    printRegister("ID (0xDC)", 0xDC, 1);  // Read ID3
    
    // Check if it might be ST7789
    Serial.println("Checking for ST7789...");
    writeCommand(0x04);  // Get ID for ST7789
    digitalWrite(TFT_DC, HIGH);
    read16(); // dummy
    uint8_t id1 = read16() & 0xFF;
    uint8_t id2 = read16() & 0xFF;
    uint8_t id3 = read16() & 0xFF;
    
    Serial.print("ST7789 Style ID: 0x");
    Serial.print(id1, HEX);
    Serial.print(" 0x");
    Serial.print(id2, HEX);
    Serial.print(" 0x");
    Serial.println(id3, HEX);
  }



  void testColorMapping() {
    Serial.println("=== Color Mapping Test ===");
    
    // Test pure colors
    Serial.println("RED (should be red)");
    fillScreen(0xF800);
    delay(2000);
    
    Serial.println("GREEN (should be green)");
    fillScreen(0x07E0);
    delay(2000);
    
    Serial.println("BLUE (should be blue)");
    fillScreen(0x001F);
    delay(2000);
    
    Serial.println("WHITE (should be white)");
    fillScreen(0xFFFF);
    delay(2000);
    
    Serial.println("BLACK (should be black)");
    fillScreen(0x0000);
    delay(2000);
    
    // Test bit patterns to identify swapped lines
    Serial.println("Bit pattern test...");
    fillScreen(0x5555); // Alternating bits
    delay(2000);
    fillScreen(0xAAAA); // Alternating bits inverted
    delay(2000);
  }

  // Color conversion helpers for RGB888 to RGB565
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }

  uint16_t color565(uint32_t color) {
    return color565((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
  }

//  Debug and test methods
  void testWindowSetting() {
    Serial.println("=== Window Setting Test ===");
    
    // Test 1: Try a simple small rectangle with alternative method
    Serial.println("Test 1: Alternative window method");
    writeCommand(0x2A); // Column Address Set
    writeData16(100);   // x0
    writeData16(150);   // x1
    
    writeCommand(0x2B); // Page Address Set  
    writeData16(100);   // y0
    writeData16(150);   // y1
    
    writeCommand(0x2C); // Memory Write
    
    // Fill small area with green
    for (int i = 0; i < 51 * 51; i++) {
      writeData16(0x07E0); // Green
    }
    
    delay(2000);
    
    // Test 2: Try the byte-separated method
    Serial.println("Test 2: Byte-separated method");
    writeCommand(0x2A); // Column Address Set
    writeData(0); writeData(200); // x0 = 200
    writeData(0); writeData(250); // x1 = 250
    
    writeCommand(0x2B); // Page Address Set
    writeData(0); writeData(100); // y0 = 100  
    writeData(0); writeData(150); // y1 = 150
    
    writeCommand(0x2C); // Memory Write
    
    // Fill with blue
    for (int i = 0; i < 51 * 51; i++) {
      writeData16(0x001F); // Blue
    }
  }
  void printStatus() {
    printRegister("Status",        0x09, 4);
    printRegister("Power Mode",    0x0A, 1);
    printRegister("MADCTL Status", 0x0B, 1);
    printRegister("Pixel Format",  0x0C, 1);
    printRegister("Image Format",  0x0D, 1);
    printRegister("Signal Mode",   0x0E, 1);
    printRegister("Self Diag",     0x0F, 1);
  }

  void testAlternativeWindowing() {
    Serial.println("Trying alternative windowing...");
    
    // Try different window approach
    writeCommand(0x2A); // Column Address Set
    writeData(0x00);
    writeData(0x00);
    writeData(0x01);
    writeData(0x3F); // 319
    
    writeCommand(0x2B); // Page Address Set  
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0xEF); // 239
    
    writeCommand(0x2C); // Memory Write
    
    // Fill with alternating pattern
    for (uint32_t i = 0; i < 320UL * 240UL; i++) {
      if (i % 2 == 0) {
        writeData16(0xF800); // Red
      } else {
        writeData16(0x07E0); // Green  
      }
    }
    
    delay(5000);
  }
};

// Example usage with Adafruit_GFX compatibility:
/*
#include <Adafruit_GFX.h>
#include "ILI9341_GFX.h"

ILI9341_GFX display;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("=== ILI9341/ST7789 Display Test ===");  
  Serial.println("Set IM pins to: IM2=1, IM1=1, IM0=0 for 16-bit parallel");
  
  display.begin();
  display.detectController();
  display.printStatus();
  
  // Now you can use all Adafruit_GFX functions:
  display.fillScreen(0x0000);                    // Black background
  display.drawLine(0, 0, 319, 239, 0xFFFF);     // White diagonal line
  display.drawRect(50, 50, 100, 80, 0xF800);    // Red rectangle
  display.fillCircle(160, 120, 30, 0x07E0);     // Green filled circle
  display.setTextColor(0x001F);                 // Blue text
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("Hello World!");
  
  // Set different rotations
  display.setRotation(1);  // Portrait
  display.setRotation(0);  // Landscape
}

void loop() {
  display.testColorMapping();
  display.testAlternativeWindowing();
}
*/