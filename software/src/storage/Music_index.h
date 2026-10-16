#pragma once
#include <stdint.h>
#include <Arduino.h>
#include <SdFat.h>  // Changed from SD.h to SdFat.h
#include <string.h>
#include "PSRAM_controller.hpp"
#include "PageStore.h"
#include "types.h" 
#include "../pinout.h"


// Maximum results to return from searches
#define MAX_SEARCH_RESULTS 50

#define NODE_BATCH_SIZE 16       // Nodes to load in one read
#define RESULT_BATCH_SIZE 8      // Results to load in one read

// Batch buffer for node loading
typedef struct {
    uint32_t start_index;
    uint32_t count;
    trie_node_t nodes[NODE_BATCH_SIZE];
} node_batch_t;

// Called for each raw trie hit, in trie order (duplicates included).
typedef void (*SearchVisitor)(const search_result_t &result, void *ctx);

class MusicIndex {
private:
    bool initialized;
    // The trie section of index.pack (read through PageStore).
    PackRef trie;

    // Offsets within the section
    uint32_t string_pool_offset;
    uint32_t string_offsets_offset;
    uint32_t nodes_offset;
    uint32_t results_offset;
    
    
    // Header data
    trie_header_t header;
    
    // The last nodes read: a node's children are contiguous, so a batch
    // read serves the sibling scans in findNode()/collectResults(). Reads go
    // through PageStore, whose PSRAM page cache is the only cache.
    node_batch_t node_batch;

    // Helper methods
    void readString(uint32_t offset, uint32_t length, char* buffer, uint32_t buffer_size);
    
    // Optimized batch loading methods
    void loadNodeBatch(uint32_t start_index, uint32_t count);
    trie_node_t* getNodeFromBatch(uint32_t node_index);
    void loadResultBatch(uint32_t start_index, search_result_t* results, uint32_t count);
    void visitResults(uint32_t start_index, uint32_t count, SearchVisitor visit, void *ctx);
    
    // Core search methods
    uint32_t findNode(const char* query, uint32_t query_len);
    void collectResults(uint32_t node_index, SearchVisitor visit, void *ctx, uint32_t* result_count, uint32_t max_results);
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
    
    // A node, from the current batch or with a fresh batch read from it.
    trie_node_t readNode(uint32_t node_index);

public:
    MusicIndex();
    ~MusicIndex();

    // Uses the search trie section of index.pack
    // (MusicLookup::getSearchTrie()).
    bool init(const PackRef &trieSection);

    // Search functions
    uint32_t searchByPrefix(const char* prefix, search_result_t* results, uint32_t max_results);
    // Same walk, handing each of the first max_results raw hits to visit
    // instead of storing them. Returns how many were visited.
    uint32_t searchByPrefix(const char* prefix, SearchVisitor visit, void *ctx, uint32_t max_results);
    uint32_t searchArtists(const char* prefix, search_result_t* results, uint32_t max_results);
    uint32_t searchAlbums(const char* prefix, search_result_t* results, uint32_t max_results);
    uint32_t searchTracks(const char* prefix, search_result_t* results, uint32_t max_results);
    uint32_t searchGenres(const char* prefix, search_result_t* results, uint32_t max_results);
    
    // Utility functions
    void getResultName(const search_result_t& result, char* buffer, uint32_t buffer_size);
    const char* getSearchTypeName(SearchType type);
    bool isInitialized() const { return initialized; }
    
    // Debug functions
    void printStats();
    void printSearchResult(const search_result_t& result);
};