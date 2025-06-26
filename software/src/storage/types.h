#pragma once
#include <Arduino.h>
typedef struct {
    uint32_t key_offset;
    uint32_t key_length;
    uint32_t child_count;
    uint32_t first_child_offset;
    uint32_t result_count;
    uint32_t first_result_offset;
} trie_node_t;

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

// Forward declarations from Music_index.h
typedef struct {
    uint32_t type;  // 0=track, 1=artist, 2=album, 3=genre
    uint32_t id;
    uint32_t name_offset;
    uint32_t name_length;
    uint32_t relevance;
} search_result_t;



