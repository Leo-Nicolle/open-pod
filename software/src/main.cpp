#include "storage/PSRAM_controller.hpp"
#include <SD.h>
#include "storage/Music_index.h"
#include <Arduino.h>

SPI_PSRAM psram;
MusicIndex musicIndex(&psram);



void searchByType(const char* query, const char* type_name, SearchType type) {
    search_result_t results[10];
    uint32_t count = 0;
    
    switch (type) {
        case SEARCH_ARTIST:
            count = musicIndex.searchArtists(query, results, 10);
            break;
        case SEARCH_ALBUM:
            count = musicIndex.searchAlbums(query, results, 10);
            break;
        case SEARCH_TRACK:
            count = musicIndex.searchTracks(query, results, 10);
            break;
        case SEARCH_GENRE:
            count = musicIndex.searchGenres(query, results, 10);
            break;
    }
    
    Serial.print(type_name);
    Serial.print(" (");
    Serial.print(count);
    Serial.print("): ");
    
    if (count > 0) {
        for (uint32_t i = 0; i < count; i++) {
            char name[64];
            musicIndex.getResultName(results[i], name, sizeof(name));
            Serial.print(name);
            if (i < count - 1) Serial.print(", ");
        }
        Serial.println();
    } else {
        Serial.println("None");
    }
}

void performSearch(const char* query) {
    Serial.print("Searching for: \"");
    Serial.print(query);
    Serial.println("\"");
    
    search_result_t results[20];
    
    // General search
    uint32_t total_results = musicIndex.searchByPrefix(query, results, 20);
    Serial.print("Total results: ");
    Serial.println(total_results);
    
    if (total_results > 0) {
        Serial.println("Results:");
        for (uint32_t i = 0; i < total_results; i++) {
            Serial.print("  ");
            Serial.print(i + 1);
            Serial.print(". ");
            musicIndex.printSearchResult(results[i]);
        }
    } else {
        Serial.println("  No results found");
    }
    
    // Type-specific searches
    Serial.println();
    searchByType(query, "Artists", SEARCH_ARTIST);
    searchByType(query, "Albums", SEARCH_ALBUM);
    searchByType(query, "Tracks", SEARCH_TRACK);
    searchByType(query, "Genres", SEARCH_GENRE);
}

void demonstrateSearch() {
    Serial.println("=== SEARCH DEMONSTRATION ===");
    
    // Test searches with common prefixes
    const char* test_queries[] = {
        "Pourquoi",
        "pourquoi",
        "Swift",
        "Django",
        "nuage",
        "sto"
    };
    
    for (int i = 0; i < 6; i++) {
        performSearch(test_queries[i]);
        Serial.println();
    }
    
    Serial.println("=== INTERACTIVE MODE ===");
    Serial.println("Type a search query and press Enter:");
}

// Additional utility functions for testing

void testPSRAMPerformance() {
    Serial.println("=== PSRAM PERFORMANCE TEST ===");
    
    const uint32_t test_size = 1024;
    uint8_t test_data[test_size];
    uint8_t read_data[test_size];
    
    // Fill test data
    for (uint32_t i = 0; i < test_size; i++) {
        test_data[i] = i & 0xFF;
    }
    
    // Write test
    unsigned long start_time = micros();
    psram.writeData(0x200000, test_data, test_size);
    unsigned long write_time = micros() - start_time;
    
    // Read test
    start_time = micros();
    psram.readData(0x200000, read_data, test_size);
    unsigned long read_time = micros() - start_time;
    
    // Verify
    bool success = true;
    for (uint32_t i = 0; i < test_size; i++) {
        if (test_data[i] != read_data[i]) {
            success = false;
            break;
        }
    }
    
    Serial.print("Write time: ");
    Serial.print(write_time);
    Serial.println(" µs");
    Serial.print("Read time: ");
    Serial.print(read_time);
    Serial.println(" µs");
    Serial.print("Write speed: ");
    Serial.print((test_size * 1000000UL) / write_time);
    Serial.println(" bytes/sec");
    Serial.print("Read speed: ");
    Serial.print((test_size * 1000000UL) / read_time);
    Serial.println(" bytes/sec");
    Serial.print("Data integrity: ");
    Serial.println(success ? "PASS" : "FAIL");
}

void printMemoryUsage() {
    Serial.println("=== MEMORY USAGE ===");
    Serial.print("Free heap: ");
    // Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    Serial.print("PSRAM capacity: ");
    Serial.print(psram.getCapacity());
    Serial.println(" bytes");
    
    if (musicIndex.isInitialized()) {
        Serial.println("Music index: Loaded in PSRAM");
    } else {
        Serial.println("Music index: Not loaded");
    }
}


void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    
    Serial.println("=== MUSIC INDEX EXAMPLE ===");
    
    // Initialize PSRAM
    Serial.println("Initializing PSRAM...");
    if (!psram.init()) {
        Serial.println("PSRAM initialization failed!");
        return;
    }
    
    // Initialize SD card
    // Serial.println("Initializing SD card...");
    // if (!SD.begin(SDCS)) {
    //     Serial.println("SD card initialization failed!");
    //     return;
    // }
    
    // Initialize music index (loads from SD card to PSRAM)
    Serial.println("Initializing music index...");
    if (!musicIndex.init("/MUSIC_~1.BIN")) {
        Serial.println("Music index initialization failed!");
        return;
    }
    
    Serial.println("All systems initialized successfully!");
    Serial.println();
    
    // Demonstrate search functionality
    demonstrateSearch();
    // musicIndex.test();
}

void loop() {
    // Interactive search demo
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        
        if (input.length() > 0) {
            performSearch(input.c_str());
        }
    }
    
    delay(100);
}

