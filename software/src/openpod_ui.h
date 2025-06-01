#pragma once
#include <Arduino.h>
#include "ILI9341_GFX.h"
#include "fonts/IBMPlexSans16.h"
#include "fonts/IBMPlexSans12.h"
#include "font-renderer.h"

// Color definitions
#define BLACK       0x0000
#define WHITE       0xFFFF
#define BLUE        0x001F
#define RED         0xF800
#define GREEN       0x07E0
#define GRAY        0x8410
#define LIGHTGRAY   0xC618
#define DARKGRAY    0x4208

class OpenPodUI {
private:
  ILI9341_GFX* display;
  
  // Track data
  const char* tracks[20] = {
    "Bohemian Rhapsody", "Hotel California", "Stairway to Heaven",
    "Sweet Child O' Mine", "Imagine", "Billie Jean",
    "Like a Rolling Stone", "Smells Like Teen Spirit", "Purple Haze",
    "What's Going On", "Respect", "Good Vibrations",
    "Johnny B. Goode", "Hey Jude", "I Want to Hold Your Hand",
    "Yesterday", "Satisfaction", "My Girl",
    "Bridge Over Troubled Water", "The Sound of Silence"
  };
  
  // UI State
  int totalTracks = 20;
  int selectedTrack = 0;
  int previousSelectedTrack = -1;
  int topVisibleTrack = 0;
  int tracksPerScreen = 6;
  
  // Layout constants
  static const int headerHeight = 30;
  static const int trackHeight = 30;
  static const int margin = 5;
  static const int trackListY = headerHeight + margin;
  
  // Performance buffers - in internal RAM for speed
  uint16_t trackBuffer[320 * 30];     // One track line
  uint16_t headerBuffer[320 * 30];    // Header area
  
  // Font renderers
  MenuItemRenderer* menuRenderer;
  FastFontRenderer* headerRenderer;
  
  // Pre-rendered static elements
  bool headerRendered = false;
  
  // Smooth scroll animation state
  bool isScrolling = false;
  int scrollOffset = 0;
  int scrollDirection = 0;
  uint32_t scrollStartTime = 0;
  
public:
  OpenPodUI(ILI9341_GFX* disp) : display(disp) {
    // Initialize font renderers
    menuRenderer = new MenuItemRenderer(trackBuffer, IBMPlexSans16, IBMPlexSans12);
    headerRenderer = new FastFontRenderer(headerBuffer, 320, 30);
  }
  
  ~OpenPodUI() {
    delete menuRenderer;
    delete headerRenderer;
  }
  
  void begin() {
    display->fillScreen(WHITE);
    renderHeader();
    renderAllTracks();
  }
  
  // Pre-render header into buffer
  void renderHeader() {
    // Fill header background
    for (int i = 0; i < 320 * 30; i++) {
      headerBuffer[i] = LIGHTGRAY;
    }
    
    // Render "OpenPod" text
    headerRenderer->renderText("OpenPod", 10, 20, IBMPlexSans16, BLACK, LIGHTGRAY);
    
    // Render battery indicator
    renderBatteryIndicator();
    
    // Push to display
    display->setWindow(0, 0, 319, 29);
    display->pushPixels(headerBuffer, 320 * 30);
    
    // Draw separator line
    display->drawFastHLine(0, headerHeight, 320, DARKGRAY);
    
    headerRendered = true;
  }
  
  // Render battery indicator
  void renderBatteryIndicator() {
    int battX = 285;
    int battY = 8;
    int battW = 25;
    int battH = 12;
    
    // Draw battery outline in buffer
    for (int y = battY; y < battY + battH; y++) {
      for (int x = battX; x < battX + battW; x++) {
        int idx = y * 320 + x;
        
        // Draw outline
        if (y == battY || y == battY + battH - 1 || 
            x == battX || x == battX + battW - 1) {
          headerBuffer[idx] = BLACK;
        }
      }
    }
    
    // Battery tip
    for (int y = battY + 3; y < battY + 9; y++) {
      headerBuffer[y * 320 + battX + battW] = BLACK;
    }
    
    // Battery fill (green)
    for (int y = battY + 2; y < battY + battH - 2; y++) {
      for (int x = battX + 2; x < battX + 18; x++) {
        headerBuffer[y * 320 + x] = GREEN;
      }
    }
  }
  
  // Render all visible tracks
  void renderAllTracks() {
    for (int i = 0; i < tracksPerScreen; i++) {
      renderTrack(i);
    }
    drawScrollIndicator();
  }
  
  // Render single track
  void renderTrack(int screenPosition) {
    int trackIndex = topVisibleTrack + screenPosition;
    if (trackIndex >= totalTracks) return;
    
    bool isSelected = (trackIndex == selectedTrack);
    int y = trackListY + (screenPosition * trackHeight);
    
    // Use MenuItemRenderer to render track
    menuRenderer->renderMenuItem(trackIndex + 1, tracks[trackIndex], isSelected);
    
    // Push to display
    display->setWindow(0, y, 319, y + trackHeight - 1);
    display->pushPixels(trackBuffer, 320 * trackHeight);
  }
  
  // Draw scroll indicator
  void drawScrollIndicator() {
    if (totalTracks <= tracksPerScreen) return;
    
    int scrollBarX = 312;
    int scrollBarY = trackListY;
    int scrollBarHeight = tracksPerScreen * trackHeight;
    
    // Clear previous indicator
    display->fillRect(scrollBarX, scrollBarY, 8, scrollBarHeight, WHITE);
    
    // Draw scroll track
    display->drawRect(scrollBarX, scrollBarY, 6, scrollBarHeight, GRAY);
    
    // Calculate thumb position
    int thumbHeight = max(20, (scrollBarHeight * tracksPerScreen) / totalTracks);
    int thumbPos = scrollBarY + ((scrollBarHeight - thumbHeight) * topVisibleTrack) / 
                   (totalTracks - tracksPerScreen);
    
    // Draw thumb
    display->fillRect(scrollBarX + 1, thumbPos, 4, thumbHeight, DARKGRAY);
  }
  
  // Update only changed tracks
  void updateChangedTracks() {
    if (previousSelectedTrack != selectedTrack) {
      // Update old selection (if visible)
      int oldPos = previousSelectedTrack - topVisibleTrack;
      if (oldPos >= 0 && oldPos < tracksPerScreen) {
        renderTrack(oldPos);
      }
      
      // Update new selection (if visible)
      int newPos = selectedTrack - topVisibleTrack;
      if (newPos >= 0 && newPos < tracksPerScreen) {
        renderTrack(newPos);
      }
      
      previousSelectedTrack = selectedTrack;
    }
  }
  
  // Smooth scroll animation
  void animateScroll() {
    if (!isScrolling) return;
    
    uint32_t elapsed = millis() - scrollStartTime;
    float progress = min(1.0f, elapsed / 100.0f);
    
    float eased = easeInOutCubic(progress);
    scrollOffset = (int)(trackHeight * eased * scrollDirection);
    
    if (progress >= 1.0f) {
      isScrolling = false;
      scrollOffset = 0;
      renderAllTracks();
    } else {
      performHardwareScroll();
    }
  }
  
  void performHardwareScroll() {
    // Calculate actual scroll position
    int baseOffset = topVisibleTrack * trackHeight;
    int animOffset = scrollOffset;
    
    // Use hardware scrolling
    display->verticalScroll(trackListY, tracksPerScreen * trackHeight, 
                           baseOffset + animOffset);
  }
  
  float easeInOutCubic(float t) {
    return t < 0.5f ? 4 * t * t * t : 1 + 4 * (t - 1) * (t - 1) * (t - 1);
  }
  
  // Navigation methods
  void scrollUp() {
    if (selectedTrack > 0) {
      previousSelectedTrack = selectedTrack;
      selectedTrack--;
      
      if (selectedTrack < topVisibleTrack) {
        topVisibleTrack--;
        renderAllTracks();
      } else {
        updateChangedTracks();
      }
    }
  }
  
  void scrollDown() {
    if (selectedTrack < totalTracks - 1) {
      previousSelectedTrack = selectedTrack;
      selectedTrack++;
      
      if (selectedTrack >= topVisibleTrack + tracksPerScreen) {
        topVisibleTrack++;
        renderAllTracks();
      } else {
        updateChangedTracks();
      }
    }
  }
  
  void pageUp() {
    int oldSelected = selectedTrack;
    int oldTop = topVisibleTrack;
    
    selectedTrack = max(0, selectedTrack - tracksPerScreen);
    topVisibleTrack = max(0, topVisibleTrack - tracksPerScreen);
    
    if (topVisibleTrack != oldTop) {
      renderAllTracks();
    } else if (selectedTrack != oldSelected) {
      previousSelectedTrack = oldSelected;
      updateChangedTracks();
    }
  }
  
  void pageDown() {
    int oldSelected = selectedTrack;
    int oldTop = topVisibleTrack;
    
    selectedTrack = min(totalTracks - 1, selectedTrack + tracksPerScreen);
    topVisibleTrack = min(totalTracks - tracksPerScreen, 
                         topVisibleTrack + tracksPerScreen);
    
    if (topVisibleTrack != oldTop) {
      renderAllTracks();
    } else if (selectedTrack != oldSelected) {
      previousSelectedTrack = oldSelected;
      updateChangedTracks();
    }
  }
  
  // Main update loop
  void update() {
    animateScroll();
  }
  
  // Select current track
  void selectTrack() {
    showNowPlaying();
  }
  
  // Show now playing screen
  void showNowPlaying() {
    display->fillScreen(WHITE);
    
    // Header
    display->fillRect(0, 0, 320, 30, LIGHTGRAY);
    display->drawFastHLine(0, 30, 320, DARKGRAY);
    
    // Use header renderer for "Now Playing"
    for (int i = 0; i < 320 * 30; i++) {
      headerBuffer[i] = LIGHTGRAY;
    }
    headerRenderer->renderText("Now Playing", 10, 20, IBMPlexSans16, BLACK, LIGHTGRAY);
    display->setWindow(0, 0, 319, 29);
    display->pushPixels(headerBuffer, 320 * 30);
    
    // Album art placeholder
    int albumX = 100;
    int albumY = 50;
    display->fillRect(albumX, albumY, 120, 120, LIGHTGRAY);
    display->drawRect(albumX, albumY, 120, 120, DARKGRAY);
    
    // Track info
    display->setTextColor(BLACK);
    display->setTextSize(2);
    display->setCursor(10, 185);
    display->print(tracks[selectedTrack]);
    
    // Progress bar
    display->drawRect(10, 220, 300, 8, DARKGRAY);
    display->fillRect(11, 221, 90, 6, GREEN);
  }
  
  void returnToList() {
    display->fillScreen(WHITE);
    renderHeader();
    renderAllTracks();
  }
  
  // Performance measurement
  void measurePerformance() {
    uint32_t start = millis();
    int operations = 0;
    
    while (millis() - start < 1000) {
      scrollDown();
      updateChangedTracks();
      operations++;
      
      if (selectedTrack >= totalTracks - 1) {
        selectedTrack = 0;
        topVisibleTrack = 0;
      }
    }
    
    Serial.print("Scroll operations per second: ");
    Serial.println(operations);
    
    start = micros();
    renderTrack(0);
    uint32_t trackTime = micros() - start;
    
    Serial.print("Single track render: ");
    Serial.print(trackTime);
    Serial.println(" microseconds");
    
    float maxFPS = 1000000.0 / (trackTime * 2);
    Serial.print("Theoretical max FPS: ");
    Serial.println(maxFPS);
  }
};

// Usage example with timing
class UIBenchmark {
public:
  static void runBenchmark(ILI9341_GFX* display) {
    Serial.println("\n=== UI Performance Benchmark ===");
    OpenPodUI ui(display);
    ui.begin();
    
    uint32_t start = micros();
    ui.scrollDown();
    ui.updateChangedTracks();
    uint32_t scrollTime = micros() - start;
    
    Serial.print("Scroll response: ");
    Serial.print(scrollTime);
    Serial.println(" µs");
    
    start = micros();
    ui.renderAllTracks();
    uint32_t fullRefresh = micros() - start;
    
    Serial.print("Full refresh: ");
    Serial.print(fullRefresh);
    Serial.println(" µs");
    
    ui.measurePerformance();
    
    float scrollFPS = 1000000.0 / scrollTime;
    Serial.print("\nSmooth scroll capability: ");
    Serial.print(scrollFPS);
    Serial.println(" FPS");
  }
};