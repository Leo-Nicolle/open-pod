#pragma once
#include <stdint.h>
#include <Arduino.h>
#include <SdFat.h>  // Changed from SD.h to SdFat.h
#include <string.h>
#include "PSRAM_controller.hpp"
#define SDCS     PA4

// Music index data structures (matches the generated trie format)
// typedef struct __attribute__((packed)) {
typedef struct {
    uint32_t key_offset;
    uint32_t key_length;
    uint32_t child_count;
    uint32_t first_child_offset;
    uint32_t result_count;
    uint32_t first_result_offset;
} trie_node_t;

typedef struct {
    uint32_t type;  // 0=track, 1=artist, 2=album, 3=genre
    uint32_t id;
    uint32_t name_offset;
    uint32_t name_length;
    uint32_t relevance;
} search_result_t;

// Header structure for the binary trie data
typedef struct {
    uint32_t string_pool_size;
    uint32_t node_count;
    uint32_t result_count;
    uint32_t string_offset_count;
} trie_header_t;

// Path index data structures
typedef struct {
    uint32_t track_count;
    uint32_t path_data_size;
} path_index_header_t;

// Search result types
enum SearchType {
    SEARCH_TRACK = 0,
    SEARCH_ARTIST = 1,
    SEARCH_ALBUM = 2,
    SEARCH_GENRE = 3
};

// Maximum results to return from searches
#define MAX_SEARCH_RESULTS 50

// Cache configuration - optimized for STM32
#define NODE_CACHE_SIZE 32      // Power of 2 for fast modulo
#define STRING_CACHE_SIZE 64    // Power of 2 for fast modulo
#define MAX_CACHED_STRING_LEN 12 // Reduced for memory efficiency
#define NODE_BATCH_SIZE 16       // Nodes to load in one PSRAM read
#define RESULT_BATCH_SIZE 8      // Results to load in one PSRAM read

// Batch buffer for node loading
typedef struct {
    uint32_t start_index;
    uint32_t count;
    trie_node_t nodes[NODE_BATCH_SIZE];
} node_batch_t;

// Enhanced cache entry with sorted index support
typedef struct {
    uint32_t node_index;
    trie_node_t node;
    uint16_t access_count;  // Reduced size, use counter instead of timestamp
    uint8_t valid;
    uint8_t _padding;       // Alignment padding
} node_cache_entry_t;

typedef struct {
    uint32_t offset;
    uint16_t length;
    uint16_t access_count;
    char data[MAX_CACHED_STRING_LEN];
    uint8_t valid;
    uint8_t _padding[3];    // Alignment padding
} string_cache_entry_t;

// Sorted index for binary search in cache
typedef struct {
    uint16_t cache_slot;
    uint32_t node_index;
} cache_index_entry_t;

class MusicIndex {
private:
    SPI_PSRAM* psram;
    uint32_t base_address;
    bool initialized;
    
    // SDFat objects
    SdFat sd;               // SDFat object for SD card operations
    
    // Offsets within the loaded data
    uint32_t string_pool_offset;
    uint32_t string_offsets_offset;
    uint32_t nodes_offset;
    uint32_t results_offset;
    
    // Path index data
    uint32_t path_index_base_address;
    uint32_t path_data_offset;
    uint32_t path_offsets_offset;
    uint32_t track_ids_offset;
    path_index_header_t path_header;
    bool path_index_initialized;
    
    // Header data
    trie_header_t header;
    
    // Cache data
    node_cache_entry_t node_cache[NODE_CACHE_SIZE];
    string_cache_entry_t string_cache[STRING_CACHE_SIZE];
    
    // Sorted index for binary search
    cache_index_entry_t node_cache_index[NODE_CACHE_SIZE];
    uint16_t node_cache_count;
    
    // Batch loading buffer
    node_batch_t node_batch;
    
    // Access counter (16-bit to save memory, wraps around)
    uint16_t access_counter;
    uint32_t readLittleEndian32(FsFile& file);  // Changed from File& to FsFile&
    
    // Helper methods
    bool loadFromSDCard(const char* filename);
    bool loadPathIndexFromSDCard(const char* filename);
    void readString(uint32_t offset, uint32_t length, char* buffer, uint32_t buffer_size);
    void readPathString(uint32_t offset, uint32_t length, char* buffer, uint32_t buffer_size);
    int32_t binarySearchTrackId(uint32_t track_id);
    
    // Optimized batch loading methods
    void loadNodeBatch(uint32_t start_index, uint32_t count);
    trie_node_t* getNodeFromBatch(uint32_t node_index);
    void loadResultBatch(uint32_t start_index, search_result_t* results, uint32_t count);
    
    // Binary search in cache
    int16_t binarySearchCache(uint32_t node_index);
    void insertCacheIndex(uint16_t slot, uint32_t node_index);
    void removeCacheIndex(uint16_t slot);
    
    // Core search methods
    uint32_t findNode(const char* query, uint32_t query_len);
    void collectResults(uint32_t node_index, search_result_t* results, uint32_t* result_count, uint32_t max_results);
    
    // String comparison (inline for speed)
    inline int compareStrings(const char* str1, const char* str2, uint32_t len) {
        while (len--) {
            uint8_t c1 = *str1 | 0x20;  // Fast lowercase
            uint8_t c2 = *str2 | 0x20;
            if (c1 != c2) return (c1 < c2) ? -1 : 1;
            if (!c1) return 0;
            str1++; str2++;
        }
        return 0;
    }
    
    inline uint32_t getCommonPrefixLength(const char* str1, const char* str2, uint32_t max_len) {
        const char* start = str1;
        while (max_len-- && *str1 && (*str1 | 0x20) == (*str2 | 0x20)) {
            str1++; str2++;
        }
        return str1 - start;
    }
    
    // Cache management methods
    void initCache();
    void clearCache();
    trie_node_t readNodeCached(uint32_t node_index);
    void readStringCached(uint32_t offset, uint32_t length, char* buffer, uint32_t buffer_size);
    uint16_t findLRUNodeCacheSlot();
    uint16_t findLRUStringCacheSlot();
    
public:
    MusicIndex(SPI_PSRAM* psram_controller, uint32_t psram_base_addr = 0x100000);
    ~MusicIndex();
    
    // Initialize the music index by loading from SD card
    bool init(const char* index_filename = "/music_index.bin");
    
    // Initialize the path index by loading from SD card
    bool initPathIndex(const char* path_index_filename = "/music_index_paths.bin");
    
    // Initialize with custom SDFat configuration
    bool initWithSDConfig(SdSpiConfig sdConfig, const char* index_filename = "/music_index.bin");
    
    // Get SDFat object reference for advanced operations
    SdFat& getSD() { return sd; }
    
    // Path lookup function
    bool getTrackPath(uint32_t track_id, char* buffer, uint32_t buffer_size);
    
    // Search functions
    uint32_t searchByPrefix(const char* prefix, search_result_t* results, uint32_t max_results);
    uint32_t searchArtists(const char* prefix, search_result_t* results, uint32_t max_results);
    uint32_t searchAlbums(const char* prefix, search_result_t* results, uint32_t max_results);
    uint32_t searchTracks(const char* prefix, search_result_t* results, uint32_t max_results);
    uint32_t searchGenres(const char* prefix, search_result_t* results, uint32_t max_results);
    
    // Relationship queries
    uint32_t getArtistsByGenre(uint32_t genre_id, search_result_t* results, uint32_t max_results);
    uint32_t getAlbumsByArtist(uint32_t artist_id, search_result_t* results, uint32_t max_results);
    uint32_t getTracksByArtist(uint32_t artist_id, search_result_t* results, uint32_t max_results);
    uint32_t getTracksByAlbum(uint32_t album_id, search_result_t* results, uint32_t max_results);
    
    // Utility functions
    void getResultName(const search_result_t& result, char* buffer, uint32_t buffer_size);
    const char* getSearchTypeName(SearchType type);
    bool isInitialized() const { return initialized; }
    
    // Debug functions
    void printStats();
    void printCacheStats();
    void printSearchResult(const search_result_t& result);
};