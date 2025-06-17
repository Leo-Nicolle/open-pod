# Music Index System for STM32

This system provides efficient music search functionality on STM32 microcontrollers using a prefix trie data structure stored in external PSRAM.

## Overview

The music index system consists of:

- **Trie-based search index**: Generated from music metadata using the Node.js indexer
- **PSRAM storage**: Binary trie data loaded from SD card to external PSRAM
- **Fast prefix search**: Optimized for real-time music search on embedded systems

## Components

### 1. PSRAM_controller.hpp

SPI interface for APS6404L-3SQR-SN 64Mbit PSRAM chip.

- Handles low-level SPI communication
- Provides read/write operations with proper timing
- Includes initialization and testing functions

### 2. Music_index.hpp/cpp

Main music index implementation.

- Loads binary trie data from SD card to PSRAM
- Provides search functions for artists, albums, tracks, and genres
- Implements prefix-based search with relevance scoring

### 3. Music_index_example.cpp

Example usage demonstrating the complete system.

## Usage

### 1. Generate Music Index

First, use the Node.js indexer to generate the binary trie data:

```bash
cd indexer
npm run example /path/to/your/music/library
```

This creates:

- `music_index.bin` - Binary trie data for STM32
- `music_index.h` - C header with data structures
- `music_index.c` - C source with embedded data (alternative to SD card)

### 2. Copy to SD Card

Copy `music_index.bin` to the root of your SD card.

### 3. Initialize in STM32 Code

```cpp
#include "Music_index.hpp"
#include "PSRAM_controller.hpp"

SPI_PSRAM psram;
MusicIndex musicIndex(&psram);

void setup() {
    // Initialize PSRAM
    if (!psram.init()) {
        Serial.println("PSRAM init failed!");
        return;
    }

    // Initialize music index (loads from SD card)
    if (!musicIndex.init("/music_index.bin")) {
        Serial.println("Music index init failed!");
        return;
    }

    Serial.println("Music index ready!");
}
```

### 4. Perform Searches

```cpp
void searchMusic(const char* query) {
    search_result_t results[20];

    // General search
    uint32_t count = musicIndex.searchByPrefix(query, results, 20);

    // Type-specific searches
    uint32_t artists = musicIndex.searchArtists(query, results, 20);
    uint32_t albums = musicIndex.searchAlbums(query, results, 20);
    uint32_t tracks = musicIndex.searchTracks(query, results, 20);

    // Print results
    for (uint32_t i = 0; i < count; i++) {
        musicIndex.printSearchResult(results[i]);
    }
}
```

## Hardware Requirements

### PSRAM Connection (SPI2)

- **CS**: PB6
- **CLK**: PB13
- **MISO**: PB14
- **MOSI**: PB15

### SD Card

Standard SPI SD card interface for loading the music index.

## Memory Layout

The system uses the following memory layout in PSRAM:

```
Base Address (0x100000):
├── Header (16 bytes)
│   ├── string_pool_size
│   ├── node_count
│   ├── result_count
│   └── string_offset_count
├── String Pool (variable size, 4-byte aligned)
├── String Offsets (4 bytes × count)
├── Trie Nodes (24 bytes × count)
└── Search Results (20 bytes × count)
```

## Data Structures

### trie_node_t

```cpp
typedef struct {
    uint32_t key_offset;        // Offset in string pool
    uint32_t key_length;        // Length of key string
    uint32_t child_count;       // Number of child nodes
    uint32_t first_child_offset; // Index of first child
    uint32_t result_count;      // Number of results at this node
    uint32_t first_result_offset; // Index of first result
} trie_node_t;
```

### search_result_t

```cpp
typedef struct {
    uint32_t type;        // 0=track, 1=artist, 2=album, 3=genre
    uint32_t id;          // Unique ID for this item
    uint32_t name_offset; // Offset in string pool
    uint32_t name_length; // Length of name string
    uint32_t relevance;   // Search relevance score (0-100)
} search_result_t;
```

## API Reference

### MusicIndex Class

#### Initialization

- `bool init(const char* filename)` - Load index from SD card to PSRAM

#### Search Functions

- `uint32_t searchByPrefix(const char* prefix, search_result_t* results, uint32_t max_results)` - General prefix search
- `uint32_t searchArtists(const char* prefix, search_result_t* results, uint32_t max_results)` - Artist-only search
- `uint32_t searchAlbums(const char* prefix, search_result_t* results, uint32_t max_results)` - Album-only search
- `uint32_t searchTracks(const char* prefix, search_result_t* results, uint32_t max_results)` - Track-only search
- `uint32_t searchGenres(const char* prefix, search_result_t* results, uint32_t max_results)` - Genre-only search

#### Utility Functions

- `void getResultName(const search_result_t& result, char* buffer, uint32_t buffer_size)` - Get result name
- `void printSearchResult(const search_result_t& result)` - Print formatted result
- `void printStats()` - Print index statistics
- `bool isInitialized()` - Check if index is loaded

## Performance

### Search Performance

- **Prefix search**: O(log n) average case
- **Result collection**: O(k) where k is number of results
- **Memory access**: Optimized for PSRAM burst reads

### Memory Usage

- **PSRAM**: ~60KB for typical music library (1000+ tracks)
- **RAM**: <2KB for search operations
- **Flash**: ~15KB for code

## Limitations

1. **Relationship queries**: Functions like `getTracksByArtist()` are placeholders and require additional metadata indexing
2. **Case sensitivity**: Currently case-insensitive search only
3. **Unicode**: Limited to ASCII characters
4. **Update frequency**: Index must be regenerated when music library changes

## Future Enhancements

1. **Relationship indexing**: Add support for artist→album→track relationships
2. **Fuzzy search**: Implement approximate string matching
3. **Real-time updates**: Support for incremental index updates
4. **Compression**: Reduce memory footprint with string compression
5. **Unicode support**: Full UTF-8 character support

## Troubleshooting

### Common Issues

1. **PSRAM init fails**: Check SPI connections and power supply
2. **SD card read fails**: Verify SD card format and file presence
3. **Search returns no results**: Check index generation and string encoding
4. **Memory corruption**: Verify PSRAM stability with extended tests

### Debug Functions

```cpp
// Test PSRAM functionality
psram.testExtended();

// Print memory usage
printMemoryUsage();

// Print index statistics
musicIndex.printStats();
```
