#include "Music_index.h"

MusicIndex::MusicIndex(SPI_PSRAM* psram_controller, uint32_t psram_base_addr) 
    : psram(psram_controller), base_address(psram_base_addr), initialized(false) {
    memset(&header, 0, sizeof(header));
}

MusicIndex::~MusicIndex() {
    // Nothing to clean up - PSRAM controller is managed externally
}

bool MusicIndex::init(const char* index_filename) {
    if (!psram) {
        Serial.println("ERROR: PSRAM controller not provided");
        return false;
    }
    Serial.println("=== MUSIC INDEX INIT ===");
    Serial.print("Loading index from: ");
    Serial.println(index_filename);
    
    // Load the binary trie data from SD card to PSRAM
    if (!loadFromSDCard(index_filename)) {
        Serial.println("ERROR: Failed to load music index from SD card");
        return false;
    }
    
    // Calculate offsets within the loaded data
    string_pool_offset = base_address + sizeof(trie_header_t);
    
    // String pool is padded to 4-byte alignment
    uint32_t string_pool_padded = header.string_pool_size;
    while (string_pool_padded % 4 != 0) {
        string_pool_padded++;
    }
    
    string_offsets_offset = string_pool_offset + string_pool_padded;
    nodes_offset = string_offsets_offset + (header.string_offset_count * 4);
    results_offset = nodes_offset + (header.node_count * sizeof(trie_node_t));
    
    initialized = true;
    
    Serial.println("Music index loaded successfully!");
    printStats();
    
    return true;
}

bool MusicIndex::loadFromSDCard(const char* filename) {
    if (!SD.begin(SDCS)) {
        Serial.println("ERROR: SD card initialization failed");
        return false;
    }

    // List the files in the root directory
    Serial.println("Listing files in root directory:");
    File root = SD.open("/");
    if (!root) {
        Serial.println("ERROR: Failed to open root directory");
        return false;
    }
    while (File file = root.openNextFile()) {
        Serial.print("File: ");
        Serial.println(file.name());
        file.close();
    }
    
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.print("ERROR: Could not open file: ");
        Serial.println(filename);
        return false;
    }
    
    uint32_t file_size = file.size();
    Serial.print("File size: ");
    Serial.print(file_size);
    Serial.println(" bytes");
    
    // Read header first
    if (file.readBytes((char*)&header, sizeof(header)) != sizeof(header)) {
        Serial.println("ERROR: Failed to read header");
        file.close();
        return false;
    }
    
    Serial.print("Header - String pool: ");
    Serial.print(header.string_pool_size);
    Serial.print(", Nodes: ");
    Serial.print(header.node_count);
    Serial.print(", Results: ");
    Serial.print(header.result_count);
    Serial.print(", String offsets: ");
    Serial.println(header.string_offset_count);
    
    // Reset file position to beginning
    file.seek(0);
    
    // Read entire file to PSRAM in chunks
    const uint32_t CHUNK_SIZE = 1024;
    uint8_t buffer[CHUNK_SIZE];
    uint32_t bytes_read = 0;
    uint32_t psram_addr = base_address;
    
    while (bytes_read < file_size) {
        uint32_t to_read = min(CHUNK_SIZE, file_size - bytes_read);
        uint32_t actual_read = file.readBytes((char*)buffer, to_read);
        
        if (actual_read == 0) {
            Serial.println("ERROR: Failed to read from file");
            file.close();
            return false;
        }
        
        psram->writeData(psram_addr, buffer, actual_read);
        psram_addr += actual_read;
        bytes_read += actual_read;
        
        // Show progress
        if (bytes_read % (CHUNK_SIZE * 4) == 0) {
            Serial.print("Loaded ");
            Serial.print(bytes_read);
            Serial.print("/");
            Serial.print(file_size);
            Serial.println(" bytes");
        }
    }
    
    file.close();
    
    Serial.print("Successfully loaded ");
    Serial.print(bytes_read);
    Serial.println(" bytes to PSRAM");
    
    return true;
}

void MusicIndex::readString(uint32_t offset, uint32_t length, char* buffer, uint32_t buffer_size) {
    uint32_t copy_len = min(length, buffer_size - 1);
    psram->readData(string_pool_offset + offset, (uint8_t*)buffer, copy_len);
    buffer[copy_len] = '\0';
}

trie_node_t MusicIndex::readNode(uint32_t node_index) {
    trie_node_t node;
    uint32_t addr = nodes_offset + (node_index * sizeof(trie_node_t));
    psram->readData(addr, (uint8_t*)&node, sizeof(node));
    return node;
}

search_result_t MusicIndex::readResult(uint32_t result_index) {
    search_result_t result;
    uint32_t addr = results_offset + (result_index * sizeof(search_result_t));
    psram->readData(addr, (uint8_t*)&result, sizeof(result));
    return result;
}

int MusicIndex::compareStrings(const char* str1, const char* str2, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        char c1 = tolower(str1[i]);
        char c2 = tolower(str2[i]);
        if (c1 < c2) return -1;
        if (c1 > c2) return 1;
        if (c1 == '\0') return 0;
    }
    return 0;
}

uint32_t MusicIndex::getCommonPrefixLength(const char* str1, const char* str2, uint32_t max_len) {
    uint32_t i = 0;
    while (i < max_len && str1[i] != '\0' && str2[i] != '\0' && 
           tolower(str1[i]) == tolower(str2[i])) {
        i++;
    }
    return i;
}

uint32_t MusicIndex::findNode(const char* query, uint32_t query_len) {
    if (!initialized || query_len == 0) {
        return UINT32_MAX; // Invalid node index
    }
    
    uint32_t current_node = 0; // Start at root
    uint32_t remaining_query_len = query_len;
    const char* remaining_query = query;
    
    while (remaining_query_len > 0) {
        trie_node_t node = readNode(current_node);
        bool found_child = false;
        
        // Check all children of current node
        for (uint32_t i = 0; i < node.child_count; i++) {
            uint32_t child_index = node.first_child_offset + i;
            trie_node_t child = readNode(child_index);
            
            // Read child's key
            char child_key[64];
            readString(child.key_offset, child.key_length, child_key, sizeof(child_key));
            
            uint32_t common_len = getCommonPrefixLength(remaining_query, child_key, 
                                                       min(remaining_query_len, child.key_length));
            
            if (common_len > 0) {
                if (common_len == child.key_length) {
                    // Full match of child key, continue with remaining query
                    current_node = child_index;
                    remaining_query += common_len;
                    remaining_query_len -= common_len;
                    found_child = true;
                    break;
                } else if (common_len == remaining_query_len) {
                    // Query is a prefix of child key - found our target node
                    return child_index;
                }
            }
        }
        
        if (!found_child) {
            return UINT32_MAX; // No matching path found
        }
    }
    
    return current_node;
}

void MusicIndex::collectResults(uint32_t node_index, search_result_t* results, 
                               uint32_t* result_count, uint32_t max_results) {
    if (*result_count >= max_results || node_index == UINT32_MAX) {
        return;
    }
    
    trie_node_t node = readNode(node_index);
    
    // Add results from this node
    for (uint32_t i = 0; i < node.result_count && *result_count < max_results; i++) {
        results[*result_count] = readResult(node.first_result_offset + i);
        (*result_count)++;
    }
    
    // Recursively collect from children
    for (uint32_t i = 0; i < node.child_count && *result_count < max_results; i++) {
        collectResults(node.first_child_offset + i, results, result_count, max_results);
    }
}

uint32_t MusicIndex::searchByPrefix(const char* prefix, search_result_t* results, uint32_t max_results) {
    if (!initialized || !prefix || !results) {
        return 0;
    }
    
    uint32_t prefix_len = strlen(prefix);
    if (prefix_len == 0) {
        return 0;
    }
    
    // Find the node that matches the prefix
    uint32_t node_index = findNode(prefix, prefix_len);
    if (node_index == UINT32_MAX) {
        return 0;
    }
    
    // Collect all results from this node and its children
    uint32_t result_count = 0;
    collectResults(node_index, results, &result_count, max_results);
    
    return result_count;
}

uint32_t MusicIndex::searchArtists(const char* prefix, search_result_t* results, uint32_t max_results) {
    search_result_t all_results[MAX_SEARCH_RESULTS];
    uint32_t total_results = searchByPrefix(prefix, all_results, MAX_SEARCH_RESULTS);
    
    uint32_t artist_count = 0;
    for (uint32_t i = 0; i < total_results && artist_count < max_results; i++) {
        if (all_results[i].type == SEARCH_ARTIST) {
            results[artist_count++] = all_results[i];
        }
    }
    
    return artist_count;
}

uint32_t MusicIndex::searchAlbums(const char* prefix, search_result_t* results, uint32_t max_results) {
    search_result_t all_results[MAX_SEARCH_RESULTS];
    uint32_t total_results = searchByPrefix(prefix, all_results, MAX_SEARCH_RESULTS);
    
    uint32_t album_count = 0;
    for (uint32_t i = 0; i < total_results && album_count < max_results; i++) {
        if (all_results[i].type == SEARCH_ALBUM) {
            results[album_count++] = all_results[i];
        }
    }
    
    return album_count;
}

uint32_t MusicIndex::searchTracks(const char* prefix, search_result_t* results, uint32_t max_results) {
    search_result_t all_results[MAX_SEARCH_RESULTS];
    uint32_t total_results = searchByPrefix(prefix, all_results, MAX_SEARCH_RESULTS);
    
    uint32_t track_count = 0;
    for (uint32_t i = 0; i < total_results && track_count < max_results; i++) {
        if (all_results[i].type == SEARCH_TRACK) {
            results[track_count++] = all_results[i];
        }
    }
    
    return track_count;
}

uint32_t MusicIndex::searchGenres(const char* prefix, search_result_t* results, uint32_t max_results) {
    search_result_t all_results[MAX_SEARCH_RESULTS];
    uint32_t total_results = searchByPrefix(prefix, all_results, MAX_SEARCH_RESULTS);
    
    uint32_t genre_count = 0;
    for (uint32_t i = 0; i < total_results && genre_count < max_results; i++) {
        if (all_results[i].type == SEARCH_GENRE) {
            results[genre_count++] = all_results[i];
        }
    }
    
    return genre_count;
}

// Note: The following relationship functions would require additional metadata
// that maps relationships between artists, albums, tracks, and genres.
// For now, they return 0 as placeholders.

uint32_t MusicIndex::getArtistsByGenre(uint32_t genre_id, search_result_t* results, uint32_t max_results) {
    // TODO: This would require a separate index mapping genres to artists
    // For now, return empty results
    Serial.println("WARNING: getArtistsByGenre not yet implemented - requires relationship metadata");
    return 0;
}

uint32_t MusicIndex::getAlbumsByArtist(uint32_t artist_id, search_result_t* results, uint32_t max_results) {
    // TODO: This would require a separate index mapping artists to albums
    Serial.println("WARNING: getAlbumsByArtist not yet implemented - requires relationship metadata");
    return 0;
}

uint32_t MusicIndex::getTracksByArtist(uint32_t artist_id, search_result_t* results, uint32_t max_results) {
    // TODO: This would require a separate index mapping artists to tracks
    Serial.println("WARNING: getTracksByArtist not yet implemented - requires relationship metadata");
    return 0;
}

uint32_t MusicIndex::getTracksByAlbum(uint32_t album_id, search_result_t* results, uint32_t max_results) {
    // TODO: This would require a separate index mapping albums to tracks
    Serial.println("WARNING: getTracksByAlbum not yet implemented - requires relationship metadata");
    return 0;
}

void MusicIndex::getResultName(const search_result_t& result, char* buffer, uint32_t buffer_size) {
    readString(result.name_offset, result.name_length, buffer, buffer_size);
}

const char* MusicIndex::getSearchTypeName(SearchType type) {
    switch (type) {
        case SEARCH_TRACK: return "Track";
        case SEARCH_ARTIST: return "Artist";
        case SEARCH_ALBUM: return "Album";
        case SEARCH_GENRE: return "Genre";
        default: return "Unknown";
    }
}

void MusicIndex::printStats() {
    if (!initialized) {
        Serial.println("Music index not initialized");
        return;
    }
    
    Serial.println("=== MUSIC INDEX STATS ===");
    Serial.print("String pool size: ");
    Serial.print(header.string_pool_size);
    Serial.println(" bytes");
    Serial.print("Node count: ");
    Serial.println(header.node_count);
    Serial.print("Result count: ");
    Serial.println(header.result_count);
    Serial.print("String offset count: ");
    Serial.println(header.string_offset_count);
    Serial.print("Base address: 0x");
    Serial.println(base_address, HEX);
    Serial.print("String pool offset: 0x");
    Serial.println(string_pool_offset, HEX);
    Serial.print("Nodes offset: 0x");
    Serial.println(nodes_offset, HEX);
    Serial.print("Results offset: 0x");
    Serial.println(results_offset, HEX);
    Serial.println("========================");
}

void MusicIndex::printSearchResult(const search_result_t& result) {
    char name[128];
    getResultName(result, name, sizeof(name));
    
    Serial.print("[");
    Serial.print(getSearchTypeName((SearchType)result.type));
    Serial.print("] ");
    Serial.print(name);
    Serial.print(" (ID: ");
    Serial.print(result.id);
    Serial.print(", Relevance: ");
    Serial.print(result.relevance);
    Serial.println(")");
}