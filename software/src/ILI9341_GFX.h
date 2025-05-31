// ILI9341 16-bit parallel driver for STM32F4 (Arduino + PlatformIO)
// CLEAN VERSION - PC0-PC15 mapping for optimal performance
// Wrapped in ILI9341_GFX class - Adafruit_GFX compatible
#include <Arduino.h>
#include <Adafruit_GFX.h>

class ILI9341_GFX : public Adafruit_GFX {
private:
  // Pin definitions - CORRECTED
  static const int TFT_DC = PB8;   // Data/Command
  static const int TFT_WR = PB9;   // Write
  static const int TFT_RD = PB10;  // Read (CORRECTED)
  static const int TFT_RST = PB12; // Reset (CORRECTED)

  // Data bus mapping: CLEAN!
  // D0-D15 → PC0-PC15 (perfect 1:1 mapping!)

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
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

    // Set all PC0-PC15 pins to output mode (data bus)
    GPIOC->MODER = 0x55555555; // Set all 16 pins to output mode (01 pattern)
    
    // Set all data pins to push-pull, high speed
    GPIOC->OTYPER = 0x0000;     // All push-pull
    GPIOC->OSPEEDR = 0xFFFFFFFF; // All high speed
  }

  inline void pulseWR() {
    GPIOB->BSRR = (1 << (9 + 16));  // WR low (PB9)
    __NOP(); // Small delay
    GPIOB->BSRR = (1 << 9);         // WR high (PB9)
  }

  inline void write16(uint16_t data) {
    // ULTRA SIMPLE - This is the dream scenario!
    GPIOC->ODR = data;  // Direct 16-bit write to PC0-PC15
    pulseWR();
  }

  void setBusToInput() {
    // Set all PC0-PC15 pins to input mode
    GPIOC->MODER = 0x00000000; // All input mode (00 pattern)
  }

  void setBusToOutput() {
    // Set all PC0-PC15 pins back to output mode
    GPIOC->MODER = 0x55555555; // All output mode (01 pattern)
  }

  uint16_t readBus16() {
    // ULTRA SIMPLE - Direct 16-bit read from PC0-PC15
    return GPIOC->IDR & 0xFFFF;
  }

  uint16_t read16() {
    setBusToInput();
    
    // Longer setup time before read
    delayMicroseconds(1);
    
    // RD low → start read
    digitalWrite(TFT_RD, LOW);
    delayMicroseconds(5); // Much longer delay for read access time
    
    uint16_t value = readBus16();
    
    // RD high → end read
    digitalWrite(TFT_RD, HIGH);
    delayMicroseconds(1); // Recovery time
    
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
    // Try different approach - some displays need different read protocol
    writeCommand(reg);
    digitalWrite(TFT_DC, HIGH);  // Read data mode
    
    // Multiple dummy reads - some displays need this
    read16(); // First dummy
    read16(); // Second dummy (sometimes needed)
    
    for (uint8_t i = 0; i < len; i++) {
      uint16_t val = read16();
      buffer[i] = val & 0xFF;
      delayMicroseconds(10); // Small delay between reads
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
    
    // Memory Access Control - CORRECTED for 320x240 landscape mode
    writeCommand(0x36);
    writeData(0xE8); // Landscape mode with proper RGB order
    
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
    writeData(x0 >> 8);   // High byte of x0
    writeData(x0 & 0xFF); // Low byte of x0
    writeData(x1 >> 8);   // High byte of x1
    writeData(x1 & 0xFF); // Low byte of x1
    
    // Page Address Set  
    writeCommand(0x2B);
    writeData(y0 >> 8);   // High byte of y0
    writeData(y0 & 0xFF); // Low byte of y0
    writeData(y1 >> 8);   // High byte of y1
    writeData(y1 & 0xFF); // Low byte of y1
    
    writeCommand(0x2C); // Memory Write
  }

public:
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
    
    setWindow(x, y, x + w - 1, y + h - 1);
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
    setWindow(0, 0, _width - 1, _height - 1);
    for (uint32_t i = 0; i < (uint32_t)_width * _height; i++) {
      writeData16(color);
    }
  }

  void setRotation(uint8_t r) override {
    rotation = r & 3;
    switch (rotation) {
      case 0: // 320x240 landscape
        _width = 320;
        _height = 240;
        writeCommand(0x36);
        writeData(0xE8); // MX=1, MY=1, MV=1 for proper 320x240
        break;
      case 1: // 240x320 portrait
        _width = 240;
        _height = 320;
        writeCommand(0x36);
        writeData(0x48); // Standard portrait
        break;
      case 2: // 320x240 landscape flipped
        _width = 320;
        _height = 240;
        writeCommand(0x36);
        writeData(0x28); // Flipped landscape
        break;
      case 3: // 240x320 portrait flipped
        _width = 240;
        _height = 320;
        writeCommand(0x36);
        writeData(0x88); // Flipped portrait
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

  void printStatus() {
    printRegister("Status",        0x09, 4);
    printRegister("Power Mode",    0x0A, 1);
    printRegister("MADCTL Status", 0x0B, 1);
    printRegister("Pixel Format",  0x0C, 1);
    printRegister("Image Format",  0x0D, 1);
    printRegister("Signal Mode",   0x0E, 1);
    printRegister("Self Diag",     0x0F, 1);
  }

  // Test methods for debugging
  void testBasicOperations() {
    Serial.println("=== Basic Operations Test ===");
    
    // Test 1: Simple data bus test
    Serial.println("Test 1: Data bus test");
    Serial.println("Writing test patterns to data bus...");
    
    digitalWrite(TFT_DC, HIGH); // Data mode
    
    // Write some test patterns
    GPIOC->ODR = 0x0000; pulseWR(); delay(100);
    GPIOC->ODR = 0xFFFF; pulseWR(); delay(100);
    GPIOC->ODR = 0x5555; pulseWR(); delay(100);
    GPIOC->ODR = 0xAAAA; pulseWR(); delay(100);
    
    Serial.println("Data bus test complete");
    
    // Test 2: Try simple fill commands
    Serial.println("Test 2: Simple fill test");
    
    // Try to fill screen with red using direct commands
    writeCommand(0x2A); // Column Address Set
    writeData(0x00); writeData(0x00); // x0 = 0
    writeData(0x01); writeData(0x3F); // x1 = 319
    
    writeCommand(0x2B); // Page Address Set
    writeData(0x00); writeData(0x00); // y0 = 0
    writeData(0x00); writeData(0xEF); // y1 = 239
    
    writeCommand(0x2C); // Memory Write
    
    Serial.println("Filling screen with red...");
    for (uint32_t i = 0; i < 320UL * 240UL; i++) {
      writeData16(0xF800); // Red
      if (i % 10000 == 0) {
        Serial.print(".");
      }
    }
    Serial.println(" Done!");
    
    delay(3000);
    
    // Test 3: Try blue
    Serial.println("Test 3: Filling screen with blue...");
    writeCommand(0x2C); // Memory Write again
    for (uint32_t i = 0; i < 320UL * 240UL; i++) {
      writeData16(0x001F); // Blue
    }
    Serial.println("Blue fill complete");
  }

  void testControlPins() {
    Serial.println("=== Control Pins Test ===");
    
    Serial.println("Testing control pin connections...");
    
    // Test reset sequence
    Serial.println("Testing RESET pin...");
    digitalWrite(TFT_RST, LOW);
    delay(100);
    digitalWrite(TFT_RST, HIGH);
    delay(100);
    
    // Test DC pin
    Serial.println("Testing DC pin...");
    digitalWrite(TFT_DC, LOW);  // Command mode
    delay(10);
    digitalWrite(TFT_DC, HIGH); // Data mode
    delay(10);
    
    // Test WR pin
    Serial.println("Testing WR pin...");
    digitalWrite(TFT_WR, LOW);
    delay(10);
    digitalWrite(TFT_WR, HIGH);
    delay(10);
    
    // Test RD pin
    Serial.println("Testing RD pin...");
    digitalWrite(TFT_RD, LOW);
    delay(10);
    digitalWrite(TFT_RD, HIGH);
    delay(10);
    
    Serial.println("Control pins test complete");
  }

  void testBasicRectangles() {
    Serial.println("=== Basic Rectangle Test ===");
    
    // First try a very simple black screen
    Serial.println("Attempting to clear screen to black...");
    
    writeCommand(0x2A); // Column Address Set
    writeData(0x00); writeData(0x00); // x0 = 0
    writeData(0x01); writeData(0x3F); // x1 = 319
    
    writeCommand(0x2B); // Page Address Set
    writeData(0x00); writeData(0x00); // y0 = 0
    writeData(0x00); writeData(0xEF); // y1 = 239
    
    writeCommand(0x2C); // Memory Write
    
    for (uint32_t i = 0; i < 320UL * 240UL; i++) {
      writeData16(0x0000); // Black
    }
    
    Serial.println("Black screen attempt complete");
    delay(2000);
    
    // Now try a small rectangle
    Serial.println("Attempting small red rectangle...");
    
    writeCommand(0x2A); // Column Address Set
    writeData(0x00); writeData(0x32); // x0 = 50
    writeData(0x00); writeData(0x64); // x1 = 100
    
    writeCommand(0x2B); // Page Address Set
    writeData(0x00); writeData(0x32); // y0 = 50
    writeData(0x00); writeData(0x64); // y1 = 100
    
    writeCommand(0x2C); // Memory Write
    
    for (int i = 0; i < 51 * 51; i++) {
      writeData16(0xF800); // Red
    }
    
    Serial.println("Small rectangle attempt complete");
    delay(2000);
  }

  void debugInitialization() {
    Serial.println("=== Debug Initialization ===");
    
    Serial.println("Step 1: Hardware reset");
    digitalWrite(TFT_RST, LOW);
    delay(20);
    digitalWrite(TFT_RST, HIGH);
    delay(150);
    Serial.println("Reset complete");
    
    Serial.println("Step 2: Software reset");
    writeCommand(0x01); // Software reset
    delay(100);
    Serial.println("Software reset complete");
    
    Serial.println("Step 3: Wake up display");
    writeCommand(0x11); // Sleep Out
    delay(120);
    Serial.println("Sleep out complete");
    
    Serial.println("Step 4: Turn on display");
    writeCommand(0x29); // Display ON
    delay(20);
    Serial.println("Display on complete");
    
    Serial.println("Step 5: Set basic parameters");
    writeCommand(0x36); // MADCTL
    writeData(0x48);    // Standard orientation
    
    writeCommand(0x3A); // Pixel Format
    writeData(0x55);    // 16-bit color
    
    Serial.println("Basic initialization complete");
  }

  // Cool demo methods
  void demoBouncingBalls() {
    Serial.println("=== Bouncing Balls Demo ===");
    
    struct Ball {
      float x, y;
      float oldX, oldY;  // Store previous position
      float vx, vy;
      uint16_t color;
      uint8_t radius;
    };
    
    Ball balls[5] = {
      {50, 50, 50, 50, 2.5, 1.8, 0xF800, 8},   // Red
      {100, 100, 100, 100, -1.9, 2.3, 0x07E0, 6}, // Green  
      {150, 80, 150, 80, 3.1, -2.1, 0x001F, 10}, // Blue
      {200, 120, 200, 120, -2.7, 1.5, 0xFFE0, 7}, // Yellow
      {80, 160, 80, 160, 1.6, -2.8, 0xF81F, 9}   // Magenta
    };
    
    // Initial screen setup
    fillScreen(0x0000);
    
    for (int frame = 0; frame < 500; frame++) {
      // First, erase old ball positions (draw black circles)
      for (int i = 0; i < 5; i++) {
        fillCircle((int)balls[i].oldX, (int)balls[i].oldY, balls[i].radius + 1, 0x0000);
      }
      
      // Update ball positions
      for (int i = 0; i < 5; i++) {
        // Store old position
        balls[i].oldX = balls[i].x;
        balls[i].oldY = balls[i].y;
        
        // Update position
        balls[i].x += balls[i].vx;
        balls[i].y += balls[i].vy;
        
        // Bounce off walls
        if (balls[i].x <= balls[i].radius || balls[i].x >= _width - balls[i].radius) {
          balls[i].vx = -balls[i].vx;
          balls[i].x = constrain(balls[i].x, balls[i].radius, _width - balls[i].radius);
        }
        if (balls[i].y <= balls[i].radius || balls[i].y >= _height - balls[i].radius) {
          balls[i].vy = -balls[i].vy;
          balls[i].y = constrain(balls[i].y, balls[i].radius, _height - balls[i].radius);
        }
      }
      
      // Now draw balls at new positions
      for (int i = 0; i < 5; i++) {
        fillCircle((int)balls[i].x, (int)balls[i].y, balls[i].radius, balls[i].color);
      }
      
      // Update frame counter (only erase the text area)
      fillRect(0, 0, 100, 20, 0x0000); // Erase just the text area
      setTextColor(0xFFFF, 0x0000);
      setTextSize(1);
      setCursor(5, 5);
      print("Frame: ");
      println(frame);
      
      delay(20); // ~50 FPS
    }
  }

  void demoColorWaves() {
    Serial.println("=== Color Waves Demo ===");
    
    for (int t = 0; t < 200; t++) {
      for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
          // Create wave patterns
          float wave1 = sin((x + t * 2) * 0.05) * sin((y + t) * 0.03);
          float wave2 = cos((x - t * 3) * 0.04) * cos((y - t * 2) * 0.06);
          
          // Map to RGB
          uint8_t r = (uint8_t)((wave1 + 1) * 127);
          uint8_t g = (uint8_t)((wave2 + 1) * 127);
          uint8_t b = (uint8_t)((sin(t * 0.1) + 1) * 127);
          
          uint16_t color = color565(r, g, b);
          drawPixel(x, y, color);
        }
      }
      
      // Show progress
      setTextColor(0xFFFF, 0x0000);
      setTextSize(2);
      setCursor(10, 10);
      print("Wave: ");
      println(t);
      
      delay(50);
    }
  }

  void demoMatrix() {
    Serial.println("=== Matrix Rain Demo ===");
    
    const int cols = _width / 8;
    int drops[cols];
    
    // Initialize drops
    for (int i = 0; i < cols; i++) {
      drops[i] = random(_height);
    }
    
    fillScreen(0x0000);
    
    for (int frame = 0; frame < 300; frame++) {
      // Fade the screen
      for (int y = 0; y < _height; y += 4) {
        for (int x = 0; x < _width; x += 4) {
          uint16_t pixel = 0; // Get pixel would be complex, so just fade to black
          drawPixel(x, y, 0x0000);
        }
      }
      
      // Draw matrix drops
      setTextSize(1);
      for (int i = 0; i < cols; i++) {
        // Random character
        char c = random(33, 127);
        
        // Bright green at the tip
        setTextColor(0x07FF, 0x0000); // Bright cyan-green
        setCursor(i * 8, drops[i] * 8);
        print(c);
        
        // Dimmer green trail
        if (drops[i] > 1) {
          setTextColor(0x03E0, 0x0000); // Dim green
          setCursor(i * 8, (drops[i] - 1) * 8);
          print((char)random(33, 127));
        }
        
        // Move drop down
        drops[i]++;
        
        // Reset drop when it reaches bottom
        if (drops[i] * 8 > _height && random(100) > 95) {
          drops[i] = 0;
        }
      }
      
      delay(100);
    }
  }

  void demoSpiral() {
    Serial.println("=== Spiral Demo ===");
    
    fillScreen(0x0000);
    
    float centerX = _width / 2.0;
    float centerY = _height / 2.0;
    
    for (int t = 0; t < 1000; t++) {
      float angle = t * 0.1;
      float radius = t * 0.15;
      
      int x = (int)(centerX + cos(angle) * radius);
      int y = (int)(centerY + sin(angle) * radius);
      
      if (x >= 0 && x < _width && y >= 0 && y < _height) {
        // Color changes with time
        uint16_t color = color565(
          (uint8_t)(sin(t * 0.01) * 127 + 128),
          (uint8_t)(sin(t * 0.013 + 2) * 127 + 128),
          (uint8_t)(sin(t * 0.017 + 4) * 127 + 128)
        );
        
        fillCircle(x, y, 3, color);
      }
      
      if (radius > max(_width, _height) / 2) {
        break;
      }
      
      delay(10);
    }
    
    delay(2000);
  }

  void demoPlasma() {
    Serial.println("=== Plasma Demo ===");
    
    for (int t = 0; t < 100; t++) {
      for (int y = 0; y < _height; y += 2) {
        for (int x = 0; x < _width; x += 2) {
          // Plasma effect math
          float v1 = sin(sqrt((x - _width/2) * (x - _width/2) + (y - _height/2) * (y - _height/2)) * 0.05 + t * 0.2);
          float v2 = sin(x * 0.03 + t * 0.15);
          float v3 = sin(y * 0.04 + t * 0.1);
          float v4 = sin(sqrt(x * x + y * y) * 0.02 + t * 0.25);
          
          float plasma = (v1 + v2 + v3 + v4) / 4.0;
          
          // Map to color
          uint8_t intensity = (uint8_t)((plasma + 1) * 127);
          uint16_t color = color565(
            intensity,
            (uint8_t)(intensity * 0.8),
            (uint8_t)(255 - intensity)
          );
          
          fillRect(x, y, 2, 2, color);
        }
      }
      
      // Show progress
      setTextColor(0xFFFF, 0x0000);
      setTextSize(1);
      setCursor(5, 5);
      print("Plasma: ");
      print(t);
      print("/100");
      
      delay(50);
    }
  }

  void demoComplete() {
    Serial.println("=== Complete Demo Sequence ===");
    
    // Title screen
    fillScreen(0x0000);
    setTextColor(0xFFFF, 0x0000);
    setTextSize(3);
    setCursor(50, 100);
    println("ILI9341");
    setTextSize(2);
    setCursor(80, 130);
    println("16-bit Parallel");
    setTextSize(1);
    setCursor(110, 160);
    println("Ultra Fast Display!");
    delay(3000);
    
    // Run all demos
    demoBouncingBalls();
    delay(1000);
    
    demoSpiral();
    delay(1000);
    
    demoMatrix();
    delay(1000);
    
    demoColorWaves();
    delay(1000);
    
    demoPlasma();
    delay(1000);
    
    // Performance test
    fillScreen(0x0000);
    setTextColor(0x07E0, 0x0000);
    setTextSize(2);
    setCursor(50, 100);
    println("Performance Test");
    
    unsigned long startTime = millis();
    for (int i = 0; i < 1000; i++) {
      fillRect(random(_width-50), random(_height-50), 50, 50, random(0xFFFF));
    }
    unsigned long endTime = millis();
    
    fillScreen(0x0000);
    setTextColor(0xFFFF, 0x0000);
    setTextSize(2);
    setCursor(20, 100);
    print("1000 rects in:");
    setCursor(20, 120);
    print(endTime - startTime);
    println(" ms");
    setCursor(20, 140);
    print("FPS: ");
    println(1000000.0 / (endTime - startTime));
    
    delay(5000);
    
    Serial.println("Demo complete!");
  }

  void testText() {
    Serial.println("=== Text Test ===");
    
    fillScreen(0x0000); // Black background
    
    // Fix text background issues by setting proper text background
    setTextColor(0xFFFF, 0x0000); // White text on black background
    setTextSize(1);
    setCursor(10, 10);
    println("Size 1: Hello World!");
    println("This line should wrap properly");
    println("Line 3");
    
    setTextSize(2);
    setCursor(10, 80);
    setTextColor(0xF800, 0x0000); // Red text on black background
    println("Size 2: Test");
    
    setTextSize(3);
    setCursor(10, 130);
    setTextColor(0x07E0, 0x0000); // Green text on black background
    println("Size 3!");
    
    delay(5000);
  }
};

// Example usage with clean PC0-PC15 mapping:
/*
#include <Adafruit_GFX.h>
#include "ILI9341_GFX.h"

ILI9341_GFX display;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("=== ILI9341 Display Test - Clean PC0-PC15 Mapping ===");  
  Serial.println("Pin mapping:");
  Serial.println("- Data bus: D0-D15 → PC0-PC15");
  Serial.println("- Control: DC=PB8, WR=PB9, RST=PB10, RD=PB12");
  Serial.println("- IM pins: IM2=0, IM1=1, IM0=1 for 16-bit parallel");
  
  display.begin();
  display.detectController();
  display.printStatus();
  
  // Test basic functionality
  display.testBasicRectangles();
  display.testText();
  
  // Now you can use all Adafruit_GFX functions perfectly:
  display.fillScreen(0x0000);                    // Black background
  display.drawLine(0, 0, 319, 239, 0xFFFF);     // White diagonal line
  display.drawRect(50, 50, 100, 80, 0xF800);    // Red rectangle
  display.fillCircle(160, 120, 30, 0x07E0);     // Green filled circle
  display.setTextColor(0x001F);                 // Blue text
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("Perfect!");
}

void loop() {
  display.testColorMapping();
  delay(1000);
}
*/