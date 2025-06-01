#pragma once
#include <Arduino.h>
#include "ILI9341_GFX.h"

// Color definitions (RGB565)
#define BLACK       0x0000
#define WHITE       0xFFFF
#define BLUE        0x001F
#define RED         0xF800
#define GREEN       0x07E0
#define CYAN        0x07FF
#define MAGENTA     0xF81F
#define YELLOW      0xFFE0
#define GRAY        0x8410
#define LIGHTGRAY   0xC618
#define DARKGRAY    0x4208
#define ORANGE      0xFC00
#define PINK        0xF81F

class OpenPodUI {
private:
  ILI9341_GFX* display;
  
  // Track list
  const char* tracks[20] = {
    "Bohemian Rhapsody",
    "Hotel California", 
    "Stairway to Heaven",
    "Sweet Child O' Mine",
    "Imagine",
    "Billie Jean",
    "Like a Rolling Stone",
    "Smells Like Teen Spirit",
    "Purple Haze",
    "What's Going On",
    "Respect",
    "Good Vibrations",
    "Johnny B. Goode",
    "Hey Jude",
    "I Want to Hold Your Hand",
    "Yesterday",
    "Satisfaction",
    "My Girl",
    "Bridge Over Troubled Water",
    "The Sound of Silence"
  };
  
  const char* artists[20] = {
    "Queen",
    "Eagles",
    "Led Zeppelin", 
    "Guns N' Roses",
    "John Lennon",
    "Michael Jackson",
    "Bob Dylan",
    "Nirvana",
    "Jimi Hendrix",
    "Marvin Gaye",
    "Aretha Franklin",
    "The Beach Boys",
    "Chuck Berry",
    "The Beatles",
    "The Beatles",
    "The Beatles",
    "The Rolling Stones",
    "The Temptations",
    "Simon & Garfunkel",
    "Simon & Garfunkel"  
  };
  
  int totalTracks = 20;
  int selectedTrack = 0;
  int topVisibleTrack = 0;
  int tracksPerScreen = 6;  // Fewer tracks per screen due to bigger text
  
  // Display parameters
  int headerHeight = 30;
  int trackHeight = 30;  // Increased height for bigger text
  int margin = 5;
  
  // Colors - Light theme
  uint16_t bgColor = WHITE;
  uint16_t headerColor = LIGHTGRAY;
  uint16_t selectedColor = BLUE;
  uint16_t textColor = BLACK;
  uint16_t secondaryTextColor = DARKGRAY;

public:
  OpenPodUI(ILI9341_GFX* disp) : display(disp) {}
  
  void begin() {
    display->fillScreen(bgColor);
    drawHeader();
    drawTrackList();
  }
  
  void drawHeader() {
    // Draw header background
    display->fillRect(0, 0, display->width(), headerHeight, headerColor);
    display->drawFastHLine(0, headerHeight, display->width(), DARKGRAY);
    
    // Draw OpenPod logo/title
    display->setTextColor(BLACK);
    display->setTextSize(2);
    display->setCursor(10, 8);
    display->print("OpenPod");
    
    // Draw battery indicator (simple rectangle)
    int batteryX = display->width() - 35;
    int batteryY = 8;
    display->drawRect(batteryX, batteryY, 25, 12, BLACK);
    display->fillRect(batteryX + 2, batteryY + 2, 18, 8, GREEN); // Battery level
    display->drawFastVLine(batteryX + 25, batteryY + 3, 6, BLACK); // Battery tip
  }
  
  void drawTrackList() {
    // Clear track list area
    display->fillRect(0, headerHeight + 1, display->width(), 
                     display->height() - headerHeight - 1, bgColor);
    
    // Calculate which tracks to show
    int startY = headerHeight + margin;
    
    for (int i = 0; i < tracksPerScreen && (topVisibleTrack + i) < totalTracks; i++) {
      int trackIndex = topVisibleTrack + i;
      int y = startY + (i * trackHeight);
      
      // Draw selection highlight
      if (trackIndex == selectedTrack) {
        display->fillRect(0, y - 2, display->width(), trackHeight, selectedColor);
      }
      
      // Draw track number
      display->setTextColor(secondaryTextColor);
      display->setTextSize(1);
      display->setCursor(margin, y);
      if (trackIndex + 1 < 10) {
        display->print(" ");
      }
      display->print(trackIndex + 1);
      display->print(".");
      
      // Draw track title (bigger text, no artist)
      display->setTextColor(trackIndex == selectedTrack ? WHITE : textColor);
      display->setTextSize(2);  // Bigger text size
      display->setCursor(margin + 25, y + 3);  // Centered vertically
      
      // Truncate long titles if needed for bigger text
      String title = String(tracks[trackIndex]);
      if (title.length() > 18) {  // Shorter limit due to bigger text
        title = title.substring(0, 15) + "...";
      }
      display->print(title);
    }
    
    // Draw scroll indicator
    drawScrollIndicator();
  }
  
  void drawScrollIndicator() {
    if (totalTracks <= tracksPerScreen) return; // No need for scroll indicator
    
    int scrollBarX = display->width() - 8;
    int scrollBarY = headerHeight + 5;
    int scrollBarHeight = display->height() - headerHeight - 10;
    
    // Draw scroll bar background
    display->drawRect(scrollBarX, scrollBarY, 6, scrollBarHeight, GRAY);
    
    // Calculate scroll thumb position and size
    int thumbHeight = max(10, (scrollBarHeight * tracksPerScreen) / totalTracks);
    int maxThumbY = scrollBarHeight - thumbHeight;
    int thumbY = scrollBarY + 1 + (maxThumbY * topVisibleTrack) / (totalTracks - tracksPerScreen);
    
    // Draw scroll thumb
    display->fillRect(scrollBarX + 1, thumbY, 4, thumbHeight, DARKGRAY);
  }
  
  void scrollUp() {
    if (selectedTrack > 0) {
      selectedTrack--;
      
      // Adjust visible window if needed
      if (selectedTrack < topVisibleTrack) {
        topVisibleTrack = selectedTrack;
      }
      
      drawTrackList();
    }
  }
  
  void scrollDown() {
    if (selectedTrack < totalTracks - 1) {
      selectedTrack++;
      
      // Adjust visible window if needed
      if (selectedTrack >= topVisibleTrack + tracksPerScreen) {
        topVisibleTrack = selectedTrack - tracksPerScreen + 1;
      }
      
      drawTrackList();
    }
  }
  
  void pageUp() {
    selectedTrack = max(0, selectedTrack - tracksPerScreen);
    topVisibleTrack = max(0, topVisibleTrack - tracksPerScreen);
    drawTrackList();
  }
  
  void pageDown() {
    selectedTrack = min(totalTracks - 1, selectedTrack + tracksPerScreen);
    if (selectedTrack >= topVisibleTrack + tracksPerScreen) {
      topVisibleTrack = min(totalTracks - tracksPerScreen, topVisibleTrack + tracksPerScreen);
    }
    drawTrackList();
  }
  
  void selectTrack() {
    // Play selected track (placeholder - implement your audio playback here)
    showNowPlaying();
  }
  
  void showNowPlaying() {
    display->fillScreen(bgColor);
    
    // Draw now playing header
    display->fillRect(0, 0, display->width(), headerHeight, LIGHTGRAY);
    display->drawFastHLine(0, headerHeight, display->width(), DARKGRAY);
    
    display->setTextColor(BLACK);
    display->setTextSize(2);
    display->setCursor(10, 8);
    display->print("Now Playing");
    
    // Draw album art placeholder
    int albumSize = 120;
    int albumX = (display->width() - albumSize) / 2;
    int albumY = headerHeight + 15;
    
    display->fillRect(albumX, albumY, albumSize, albumSize, LIGHTGRAY);
    display->drawRect(albumX, albumY, albumSize, albumSize, DARKGRAY);
    
    // Draw musical note icon
    display->setTextColor(DARKGRAY);
    display->setTextSize(4);
    display->setCursor(albumX + albumSize/2 - 12, albumY + albumSize/2 - 16);
    display->print("♪");
    
    // Draw track info
    int infoY = albumY + albumSize + 15;
    
    display->setTextColor(BLACK);
    display->setTextSize(1);
    display->setCursor(margin, infoY);
    
    String title = String(tracks[selectedTrack]);
    if (title.length() > 40) {
      title = title.substring(0, 37) + "...";
    }
    display->print(title);
    
    display->setTextColor(DARKGRAY);
    display->setCursor(margin, infoY + 15);
    String artist = String(artists[selectedTrack]);
    if (artist.length() > 40) {
      artist = artist.substring(0, 37) + "...";
    }
    display->print(artist);
    
    // Draw progress bar
    int progressY = infoY + 35;
    int progressWidth = display->width() - (2 * margin);
    display->drawRect(margin, progressY, progressWidth, 8, DARKGRAY);
    display->fillRect(margin + 1, progressY + 1, progressWidth * 0.3, 6, GREEN); // 30% progress
    
    // Draw time
    display->setTextColor(DARKGRAY);
    display->setTextSize(1);
    display->setCursor(margin, progressY + 15);
    display->print("1:23");
    
    display->setCursor(display->width() - 30, progressY + 15);
    display->print("4:15");
  }
  
  void returnToList() {
    display->fillScreen(bgColor);
    drawHeader();
    drawTrackList();
  }
  
  // Getters
  int getSelectedTrack() { return selectedTrack; }
  const char* getCurrentTrackName() { return tracks[selectedTrack]; }
  const char* getCurrentArtistName() { return artists[selectedTrack]; }
  
  // Setters
  void setSelectedTrack(int track) {
    if (track >= 0 && track < totalTracks) {
      selectedTrack = track;
      
      // Adjust visible window
      if (selectedTrack < topVisibleTrack) {
        topVisibleTrack = selectedTrack;
      } else if (selectedTrack >= topVisibleTrack + tracksPerScreen) {
        topVisibleTrack = selectedTrack - tracksPerScreen + 1;
      }
      
      drawTrackList();
    }
  }
};