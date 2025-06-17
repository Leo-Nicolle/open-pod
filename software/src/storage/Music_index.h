#pragma once
#include <stdint.h>
#include <Arduino.h>
#include <SD.h>
#include <string.h>
#include "PSRAM_controller.hpp"
#define SDCS     PA4 


// Music index data structures (matches the generated trie format)
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

// Search result types
enum SearchType {
    SEARCH_TRACK = 0,
    SEARCH_ARTIST = 1,
    SEARCH_ALBUM = 2,
    SEARCH_GENRE = 3
};

// Maximum results to return from searches
#define MAX_SEARCH_RESULTS 50

class MusicIndex {
private:
    SPI_PSRAM* psram;
    uint32_t base_address;
    bool initialized;
    
    // Offsets within the loaded data
    uint32_t string_pool_offset;
    uint32_t string_offsets_offset;
    uint32_t nodes_offset;
    uint32_t results_offset;
    
    // Header data
    trie_header_t header;
    
    // Helper methods
    bool loadFromSDCard(const char* filename);
    void readString(uint32_t offset, uint32_t length, char* buffer, uint32_t buffer_size);
    trie_node_t readNode(uint32_t node_index);
    search_result_t readResult(uint32_t result_index);
    uint32_t findNode(const char* query, uint32_t query_len);
    void collectResults(uint32_t node_index, search_result_t* results, uint32_t* result_count, uint32_t max_results);
    int compareStrings(const char* str1, const char* str2, uint32_t len);
    uint32_t getCommonPrefixLength(const char* str1, const char* str2, uint32_t max_len);
    
public:
    MusicIndex(SPI_PSRAM* psram_controller, uint32_t psram_base_addr = 0x100000);
    ~MusicIndex();
    
    // Initialize the music index by loading from SD card
    bool init(const char* index_filename = "/music_index.bin");
    
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
    void printSearchResult(const search_result_t& result);
};
