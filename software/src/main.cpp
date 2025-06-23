/*!
 * @file main.cpp
 * FIXED: Smooth VS1053 audio player for STM32 - no more glitches
 */

#include <Arduino.h>
#include "sound/VS1053_driver.h"

// STM32 Nucleo F446RE pin assignments
#define VS1053_RST    PA10   // Reset pin
#define VS1053_CS     PA11   // Control SPI chip select  
#define VS1053_DCS    PA9    // Data SPI chip select
#define VS1053_DREQ   PB3    // Data request pin (CRITICAL: Use interrupt-capable pin)
#define SD_CS         PA4    // SD card chip select (SPI1)

// Create VS1053 player
VS1053FilePlayer player(VS1053_RST, VS1053_CS, VS1053_DCS, VS1053_DREQ, SD_CS);

// Performance monitoring
volatile uint32_t feedCount = 0;
volatile uint32_t isrCount = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== FIXED VS1053 Audio Player ===");
    Serial.println("Initializing...");
    
    // CRITICAL: Lower SPI speeds initially for reliability
    if (!player.begin(1000000, 16000000)) {  // VS1053: 1MHz, SD: 16MHz
        Serial.println("FAILED to initialize!");
        while (1) {
            delay(5000);
        }
    }
    
    Serial.println("✓ VS1053 and SD initialized");
    
    // Test basic functionality first
    Serial.println("Testing VS1053 with sine wave...");
    player.sineTest(0x44, 500);  // 1kHz sine for 500ms
    delay(500);
    
    // Initialize PSRAM if available
    if (player.enablePSRAM(true)) {
        Serial.println("✓ PSRAM available");
    } else {
        Serial.println("⚠ No PSRAM - using direct streaming");
    }
    
    player.setVolume(40, 40);  // More conservative volume
    
    Serial.println("\nStarting playback...");
    
    // FIXED: Always try PSRAM preload first if available
    bool usePreload = player.isPSRAMAvailable();
    
    if (player.startPlaying("track-3.mp3", usePreload)) {
        if (usePreload) {
            Serial.println("✓ Playing from PSRAM preload");
        } else {
            Serial.println("✓ Playing from SD streaming");
        }
        Serial.println("Interrupts will handle feeding automatically");
    } else {
        Serial.println("✗ Playback failed");
        Serial.println("Check SD card and file existence");
        while(1) delay(1000);
    }
}

void loop() {
    static unsigned long lastReport = 0;
    static uint16_t lastDecodeTime = 0;
    static uint32_t lastFeedCount = 0;
    static uint32_t lastIsrCount = 0;
    static uint32_t stallCount = 0;
    
    // CRITICAL: Process any deferred operations
    player.processDeferred();
    
    // FIXED: Only help if interrupts are failing
    if (player.isPlaying() && player.readyForData()) {
        // This should rarely happen if interrupts work properly
        feedCount++;
        player.feedBuffer();
    }
    
    // Detailed status every second
    unsigned long now = millis();
    if (now - lastReport >= 1000) {
        lastReport = now;
        
        if (player.isPlaying()) {
            uint16_t decodeTime = player.getDecodeTime();
            uint32_t feedRate = feedCount - lastFeedCount;
            uint32_t isrRate = isrCount - lastIsrCount;
            
            // Check for stalls
            bool isStalled = (decodeTime == lastDecodeTime);
            if (isStalled) stallCount++;
            
            // Serial.printf("Time: %ds | Feeds: %d/s | ISR: %d/s | DREQ: %s", 
            //              decodeTime, feedRate, isrRate,
            //              digitalRead(VS1053_DREQ) ? "HI" : "LO");
            
            // if (player.isPSRAMAvailable()) {
            //     Serial.printf(" | PSRAM: %d bytes", player.getPreloadedRemaining());
            // }
            
            if (isStalled) {
                // Serial.printf(" | STALL #%d ⚠", stallCount);
                
                // Recovery attempt for stalls
                if (stallCount > 3) {
                    // Serial.println("\nToo many stalls - attempting recovery...");
                    // Force feed some data
                    for (int i = 0; i < 10 && player.readyForData(); i++) {
                        player.feedBuffer();
                    }
                    stallCount = 0;
                }
            } else {
                // Serial.println(" ✓");
                stallCount = 0;  // Reset stall count on good progress
            }
            
            lastDecodeTime = decodeTime;
            lastFeedCount = feedCount;
            lastIsrCount = isrCount;
            
            // Debug register dump on problems
            if (stallCount > 5) {
                // Serial.println("Register dump:");
                player.dumpRegisters();
                stallCount = 0;
            }
            
        } else {
            // Serial.println("Playback finished");
            
            // Auto-restart for testing
            delay(2000);
            // Serial.println("Restarting...");
            if (player.startPlaying("track-1.flac", player.isPSRAMAvailable())) {
                // Serial.println("Restarted playback");
            }
        }
    }
    
    // CRITICAL: Don't hog the CPU
    delay(1);  // Small delay to prevent overwhelming the system
}

// Optional: Track ISR calls for debugging
void trackISR() {
    isrCount++;
}