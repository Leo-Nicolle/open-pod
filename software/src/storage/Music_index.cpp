#include "Music_index.h"

MusicIndex::MusicIndex(SPI_PSRAM *psram_controller, uint32_t psram_base_addr)
    : psram(psram_controller), base_address(psram_base_addr),
      initialized(false), path_index_initialized(false), node_cache_count(0), access_counter(0) {
  memset(&header, 0, sizeof(header));
  memset(&path_header, 0, sizeof(path_header));
  memset(&node_batch, 0, sizeof(node_batch));
  path_index_base_address = psram_base_addr + 0x200000; // Offset path index by 2MB
  initCache();
}

MusicIndex::~MusicIndex() {
  // Nothing to clean up - PSRAM controller is managed externally
}

bool MusicIndex::init(const char *index_filename) {
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
  uint32_t string_pool_padded = (header.string_pool_size); //  + 3) & ~3;

  string_offsets_offset = string_pool_offset + string_pool_padded;
  nodes_offset = string_offsets_offset + (header.string_offset_count * 4);
  results_offset = nodes_offset + (header.node_count * sizeof(trie_node_t));

  initialized = true;

  // Clear cache since we loaded new data
  clearCache();

  Serial.println("Music index loaded successfully!");
  printStats();

  return true;
}

bool MusicIndex::initPathIndex(const char *path_index_filename) {
  if (!psram) {
    Serial.println("ERROR: PSRAM controller not provided for path index");
    return false;
  }
  
  Serial.println("=== PATH INDEX INIT ===");
  Serial.print("Loading path index from: ");
  Serial.println(path_index_filename);

  // Load the binary path index data from SD card to PSRAM
  if (!loadPathIndexFromSDCard(path_index_filename)) {
    Serial.println("ERROR: Failed to load path index from SD card");
    return false;
  }

  path_index_initialized = true;
  Serial.println("Path index loaded successfully!");
  
  return true;
}

bool MusicIndex::getTrackPath(uint32_t track_id, char *buffer, uint32_t buffer_size) {
  if (!path_index_initialized || !buffer || buffer_size == 0) {
    return false;
  }

  // Binary search for track ID
  int32_t index = binarySearchTrackId(track_id);
  if (index < 0) {
    return false; // Track ID not found
  }

  // Read path offset for this track
  uint32_t path_offset;
  uint32_t offset_addr = path_offsets_offset + (index * 4);
  psram->readData(offset_addr, (uint8_t *)&path_offset, 4);

  // Calculate path length
  uint32_t path_length;
  if (index + 1 < path_header.track_count) {
    // Read next offset to calculate length
    uint32_t next_offset;
    uint32_t next_offset_addr = path_offsets_offset + ((index + 1) * 4);
    psram->readData(next_offset_addr, (uint8_t *)&next_offset, 4);
    path_length = next_offset - path_offset - 1; // -1 for null terminator
  } else {
    // Last entry, calculate from total path data size
    path_length = path_header.path_data_size - path_offset - 1;
  }

  // Read the path string
  readPathString(path_offset, path_length, buffer, buffer_size);
  
  return true;
}

uint32_t MusicIndex::readLittleEndian32(File &file) {
  uint8_t bytes[4];
  file.readBytes((char *)bytes, 4);
  return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
         ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

bool MusicIndex::loadFromSDCard(const char *filename) {
  if (!SD.begin(SDCS)) {
    Serial.println("ERROR: SD card initialization failed");
    return false;
  }
  // print all files in the root directory
  // File root = SD.open("/");
  // Serial.println("Files in root directory:");
  // while (File file = root.openNextFile()) {
  //   Serial.print(" - ");
  //   Serial.println(file.name());
  //   file.close();
  // }
  // root.close(); 


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

  // Read entire file to PSRAM in larger chunks for speed
  const uint32_t CHUNK_SIZE = 4096; // Increased chunk size
  uint8_t *buffer = (uint8_t *)malloc(CHUNK_SIZE);
  if (!buffer) {
    Serial.println("ERROR: Failed to allocate buffer");
    file.close();
    return false;
  }

  uint32_t bytes_read = 0;
  uint32_t psram_addr = base_address;

  while (bytes_read < file_size) {
    uint32_t to_read = min(CHUNK_SIZE, file_size - bytes_read);
    uint32_t actual_read = file.readBytes((char *)buffer, to_read);

    if (actual_read == 0) {
      Serial.println("ERROR: Failed to read from file");
      free(buffer);
      file.close();
      return false;
    }

    psram->writeData(psram_addr, buffer, actual_read);
    psram_addr += actual_read;
    bytes_read += actual_read;

    // Show progress every 16KB
    if (bytes_read % 16384 == 0) {
      Serial.print("Loaded ");
      Serial.print(bytes_read);
      Serial.print("/");
      Serial.print(file_size);
      Serial.println(" bytes");
    }
  }

  free(buffer);
  file.close();

  Serial.print("Successfully loaded ");
  Serial.print(bytes_read);
  Serial.println(" bytes to PSRAM");

  return true;
}

bool MusicIndex::loadPathIndexFromSDCard(const char *filename) {
  if (!SD.begin(SDCS)) {
    Serial.println("ERROR: SD card initialization failed for path index");
    return false;
  }

  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.print("ERROR: Could not open path index file: ");
    Serial.println(filename);
    return false;
  }

  uint32_t file_size = file.size();
  Serial.print("Path index file size: ");
  Serial.print(file_size);
  Serial.println(" bytes");

  // Read path index header
  if (file.readBytes((char*)&path_header, sizeof(path_header)) != sizeof(path_header)) {
    Serial.println("ERROR: Failed to read path index header");
    file.close();
    return false;
  }

  Serial.print("Path Index Header - Track count: ");
  Serial.print(path_header.track_count);
  Serial.print(", Path data size: ");
  Serial.println(path_header.path_data_size);

  // Reset file position to beginning
  file.seek(0);

  // Read entire path index file to PSRAM
  const uint32_t CHUNK_SIZE = 4096;
  uint8_t *buffer = (uint8_t *)malloc(CHUNK_SIZE);
  if (!buffer) {
    Serial.println("ERROR: Failed to allocate buffer for path index");
    file.close();
    return false;
  }

  uint32_t bytes_read = 0;
  uint32_t psram_addr = path_index_base_address;

  while (bytes_read < file_size) {
    uint32_t to_read = min(CHUNK_SIZE, file_size - bytes_read);
    uint32_t actual_read = file.readBytes((char *)buffer, to_read);

    if (actual_read == 0) {
      Serial.println("ERROR: Failed to read from path index file");
      free(buffer);
      file.close();
      return false;
    }

    psram->writeData(psram_addr, buffer, actual_read);
    psram_addr += actual_read;
    bytes_read += actual_read;
  }

  free(buffer);
  file.close();

  // Calculate offsets within the loaded path index data
  track_ids_offset = path_index_base_address + sizeof(path_index_header_t);
  path_offsets_offset = track_ids_offset + (path_header.track_count * 4);
  path_data_offset = path_offsets_offset + (path_header.track_count * 4);

  Serial.print("Successfully loaded path index ");
  Serial.print(bytes_read);
  Serial.println(" bytes to PSRAM");

  return true;
}

void MusicIndex::readString(uint32_t offset, uint32_t length, char *buffer,
                             uint32_t buffer_size) {
  uint32_t copy_len = min(length, buffer_size - 1);
  psram->readData(string_pool_offset + offset, (uint8_t *)buffer, copy_len);
  buffer[copy_len] = '\0';
}

void MusicIndex::readPathString(uint32_t offset, uint32_t length, char *buffer,
                                uint32_t buffer_size) {
  uint32_t copy_len = min(length, buffer_size - 1);
  // TODO: Find the origin of this +4 offset
  psram->readData(path_data_offset + offset+4, (uint8_t *)buffer, copy_len);
  buffer[copy_len] = '\0';
}

int32_t MusicIndex::binarySearchTrackId(uint32_t track_id) {
  if (!path_index_initialized) {
    return -1;
  }

  uint32_t left = 0;
  uint32_t right = path_header.track_count - 1;

  while (left <= right) {
    uint32_t mid = (left + right) / 2;
    
    // Read track ID at mid position
    uint32_t mid_track_id;
    uint32_t addr = track_ids_offset + (mid * 4);
    psram->readData(addr, (uint8_t *)&mid_track_id, 4);

    if (mid_track_id == track_id) {
      return (int32_t)mid;
    } else if (mid_track_id < track_id) {
      left = mid + 1;
    } else {
      if (mid == 0) break; // Prevent underflow
      right = mid - 1;
    }
  }

  return -1; // Not found
}

void MusicIndex::loadNodeBatch(uint32_t start_index, uint32_t count) {
  // Limit count to batch size
  count = min(count, (uint32_t)NODE_BATCH_SIZE);

  // Update batch info
  node_batch.start_index = start_index;
  node_batch.count = count;

  // Read all nodes in one PSRAM operation
  uint32_t addr = nodes_offset + (start_index * sizeof(trie_node_t));
  psram->readData(addr, (uint8_t *)node_batch.nodes,
                  count * sizeof(trie_node_t));
}

trie_node_t *MusicIndex::getNodeFromBatch(uint32_t node_index) {
  // Check if node is in current batch
  if (node_index >= node_batch.start_index &&
      node_index < node_batch.start_index + node_batch.count) {
    return &node_batch.nodes[node_index - node_batch.start_index];
  }
  return nullptr;
}

void MusicIndex::loadResultBatch(uint32_t start_index, search_result_t *results,
                                 uint32_t count) {
  // Read all results in one PSRAM operation
  uint32_t addr = results_offset + (start_index * sizeof(search_result_t));
  psram->readData(addr, (uint8_t *)results, count * sizeof(search_result_t));
}

int16_t MusicIndex::binarySearchCache(uint32_t node_index) {
  if (node_cache_count == 0)
    return -1;

  uint16_t left = 0;
  uint16_t right = node_cache_count - 1;

  while (left <= right) {
    uint16_t mid = (left + right) >> 1; // Fast division by 2
    uint32_t mid_index = node_cache_index[mid].node_index;

    if (mid_index == node_index) {
      return node_cache_index[mid].cache_slot;
    } else if (mid_index < node_index) {
      left = mid + 1;
    } else {
      if (mid == 0)
        break; // Prevent underflow
      right = mid - 1;
    }
  }

  return -1; // Not found
}

void MusicIndex::insertCacheIndex(uint16_t slot, uint32_t node_index) {
  // Find insertion position using binary search
  uint16_t pos = 0;
  uint16_t left = 0;
  uint16_t right = node_cache_count;

  while (left < right) {
    uint16_t mid = (left + right) >> 1;
    if (node_cache_index[mid].node_index < node_index) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }
  pos = left;

  // Shift elements if needed
  if (pos < node_cache_count) {
    memmove(&node_cache_index[pos + 1], &node_cache_index[pos],
            (node_cache_count - pos) * sizeof(cache_index_entry_t));
  }

  // Insert new entry
  node_cache_index[pos].cache_slot = slot;
  node_cache_index[pos].node_index = node_index;
  node_cache_count++;
}

void MusicIndex::removeCacheIndex(uint16_t slot) {
  // Find the entry with this slot
  for (uint16_t i = 0; i < node_cache_count; i++) {
    if (node_cache_index[i].cache_slot == slot) {
      // Remove by shifting elements
      if (i < node_cache_count - 1) {
        memmove(&node_cache_index[i], &node_cache_index[i + 1],
                (node_cache_count - i - 1) * sizeof(cache_index_entry_t));
      }
      node_cache_count--;
      break;
    }
  }
}

void MusicIndex::initCache() {
  // Initialize node cache
  memset(node_cache, 0, sizeof(node_cache));
  memset(node_cache_index, 0, sizeof(node_cache_index));
  node_cache_count = 0;

  // Initialize string cache
  memset(string_cache, 0, sizeof(string_cache));

  // Initialize batch buffer
  node_batch.start_index = UINT32_MAX;
  node_batch.count = 0;
}

void MusicIndex::clearCache() { initCache(); }

trie_node_t MusicIndex::readNodeCached(uint32_t node_index) {
  access_counter++;

  // First check if it's in the current batch
  trie_node_t *batch_node = getNodeFromBatch(node_index);
  if (batch_node) {
    return *batch_node;
  }

  // Binary search in cache
  int16_t cache_slot = binarySearchCache(node_index);

  if (cache_slot >= 0) {
    // Cache hit - update access count
    node_cache[cache_slot].access_count = access_counter;
    return node_cache[cache_slot].node;
  }

  // Cache miss - need to load from PSRAM
  // Try to predict and load adjacent nodes too
  uint32_t batch_start = node_index;
  uint32_t batch_count = NODE_BATCH_SIZE;

  // Adjust batch to not exceed total nodes
  if (batch_start + batch_count > header.node_count) {
    batch_count = header.node_count - batch_start;
  }

  // Load batch
  loadNodeBatch(batch_start, batch_count);

  // Get the requested node from batch
  batch_node = getNodeFromBatch(node_index);
  if (!batch_node) {
    // Shouldn't happen, but handle gracefully
    trie_node_t single_node;
    uint32_t addr = nodes_offset + (node_index * sizeof(trie_node_t));
    psram->readData(addr, (uint8_t *)&single_node, sizeof(trie_node_t));
    return single_node;
  }

  // Add to cache
  uint16_t lru_slot = findLRUNodeCacheSlot();

  // Remove old entry from index if slot was occupied
  if (node_cache[lru_slot].valid) {
    removeCacheIndex(lru_slot);
  }

  // Cache the node
  node_cache[lru_slot].valid = 1;
  node_cache[lru_slot].node_index = node_index;
  node_cache[lru_slot].node = *batch_node;
  node_cache[lru_slot].access_count = access_counter;

  // Add to sorted index
  insertCacheIndex(lru_slot, node_index);

  return *batch_node;
}

void MusicIndex::readStringCached(uint32_t offset, uint32_t length,
                                  char *buffer, uint32_t buffer_size) {
  access_counter++;

  // Only cache strings that fit in our cache buffer
  if (length >= MAX_CACHED_STRING_LEN) {
    readString(offset, length, buffer, buffer_size);
    return;
  }

  // Simple hash for fast lookup (offset XOR length)
  uint32_t hash = (offset ^ length) & (STRING_CACHE_SIZE - 1);

  // Check direct hash slot first
  if (string_cache[hash].valid && string_cache[hash].offset == offset &&
      string_cache[hash].length == length) {
    // Cache hit
    string_cache[hash].access_count = access_counter;
    uint32_t copy_len = min(length, buffer_size - 1);
    memcpy(buffer, string_cache[hash].data, copy_len);
    buffer[copy_len] = '\0';
    return;
  }

  // Linear probe for collision resolution (up to 4 slots)
  for (uint8_t i = 1; i < 4; i++) {
    uint32_t slot = (hash + i) & (STRING_CACHE_SIZE - 1);
    if (string_cache[slot].valid && string_cache[slot].offset == offset &&
        string_cache[slot].length == length) {
      // Cache hit
      string_cache[slot].access_count = access_counter;
      uint32_t copy_len = min(length, buffer_size - 1);
      memcpy(buffer, string_cache[slot].data, copy_len);
      buffer[copy_len] = '\0';
      return;
    }
  }

  // Cache miss - read from PSRAM
  readString(offset, length, buffer, buffer_size);

  // Find slot to cache (prefer empty or LRU in probed range)
  uint16_t lru_slot = hash;
  uint16_t min_access = string_cache[hash].access_count;

  for (uint8_t i = 0; i < 4; i++) {
    uint32_t slot = (hash + i) & (STRING_CACHE_SIZE - 1);
    if (!string_cache[slot].valid) {
      lru_slot = slot;
      break;
    }
    if (string_cache[slot].access_count < min_access) {
      min_access = string_cache[slot].access_count;
      lru_slot = slot;
    }
  }

  // Cache the string
  string_cache[lru_slot].valid = 1;
  string_cache[lru_slot].offset = offset;
  string_cache[lru_slot].length = length;
  string_cache[lru_slot].access_count = access_counter;

  uint32_t copy_len = min(length, (uint32_t)(MAX_CACHED_STRING_LEN - 1));
  memcpy(string_cache[lru_slot].data, buffer, copy_len);
  string_cache[lru_slot].data[copy_len] = '\0';
}

uint16_t MusicIndex::findLRUNodeCacheSlot() {
  uint16_t lru_slot = 0;
  uint16_t min_access = node_cache[0].access_count;

  // Quick scan for empty slot
  for (uint16_t i = 0; i < NODE_CACHE_SIZE; i++) {
    if (!node_cache[i].valid) {
      return i;
    }
    if (node_cache[i].access_count < min_access) {
      min_access = node_cache[i].access_count;
      lru_slot = i;
    }
  }

  return lru_slot;
}

uint16_t MusicIndex::findLRUStringCacheSlot() {
  uint16_t lru_slot = 0;
  uint16_t min_access = string_cache[0].access_count;

  for (uint16_t i = 1; i < STRING_CACHE_SIZE; i++) {
    if (!string_cache[i].valid) {
      return i;
    }
    if (string_cache[i].access_count < min_access) {
      min_access = string_cache[i].access_count;
      lru_slot = i;
    }
  }

  return lru_slot;
}

uint32_t MusicIndex::findNode(const char *query, uint32_t query_len) {
  if (!initialized || query_len == 0) {
    return UINT32_MAX;
  }

  uint32_t current_node = 0;
  uint32_t remaining_query_len = query_len;
  const char *remaining_query = query;

  while (remaining_query_len > 0) {
    trie_node_t node = readNodeCached(current_node);
    bool found_child = false;

    // Load all children in a batch if there are many
    if (node.child_count > 4) {
      loadNodeBatch(node.first_child_offset,
                    min(node.child_count, (uint32_t)NODE_BATCH_SIZE));
    }

    // Check all children of current node
    for (uint32_t i = 0; i < node.child_count; i++) {
      uint32_t child_index = node.first_child_offset + i;
      trie_node_t child = readNodeCached(child_index);

      // Read child's key
      char child_key[64];
      readStringCached(child.key_offset, child.key_length, child_key,
                       sizeof(child_key));

      uint32_t common_len =
          getCommonPrefixLength(remaining_query, child_key,
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
      return UINT32_MAX;
    }
  }

  return current_node;
}

void MusicIndex::collectResults(uint32_t node_index, search_result_t *results,
                                uint32_t *result_count, uint32_t max_results) {
  if (*result_count >= max_results || node_index == UINT32_MAX) {
    return;
  }

  trie_node_t node = readNodeCached(node_index);

  // Batch load results if there are many
  if (node.result_count > 0) {
    uint32_t results_to_load =
        min(node.result_count, max_results - *result_count);
    loadResultBatch(node.first_result_offset, &results[*result_count],
                    results_to_load);
    *result_count += results_to_load;
  }

  // Batch load children for recursive collection
  if (node.child_count > 0 && *result_count < max_results) {
    loadNodeBatch(node.first_child_offset,
                  min(node.child_count, (uint32_t)NODE_BATCH_SIZE));

    // Recursively collect from children
    for (uint32_t i = 0; i < node.child_count && *result_count < max_results;
         i++) {
      collectResults(node.first_child_offset + i, results, result_count,
                     max_results);
    }
  }
}

uint32_t MusicIndex::searchByPrefix(const char *prefix,
                                    search_result_t *results,
                                    uint32_t max_results) {
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

uint32_t MusicIndex::searchArtists(const char *prefix, search_result_t *results,
                                   uint32_t max_results) {
  search_result_t all_results[MAX_SEARCH_RESULTS];
  uint32_t total_results =
      searchByPrefix(prefix, all_results, MAX_SEARCH_RESULTS);

  uint32_t artist_count = 0;
  for (uint32_t i = 0; i < total_results && artist_count < max_results; i++) {
    if (all_results[i].type == SEARCH_ARTIST) {
      results[artist_count++] = all_results[i];
    }
  }

  return artist_count;
}

uint32_t MusicIndex::searchAlbums(const char *prefix, search_result_t *results,
                                  uint32_t max_results) {
  search_result_t all_results[MAX_SEARCH_RESULTS];
  uint32_t total_results =
      searchByPrefix(prefix, all_results, MAX_SEARCH_RESULTS);

  uint32_t album_count = 0;
  for (uint32_t i = 0; i < total_results && album_count < max_results; i++) {
    if (all_results[i].type == SEARCH_ALBUM) {
      results[album_count++] = all_results[i];
    }
  }

  return album_count;
}

uint32_t MusicIndex::searchTracks(const char *prefix, search_result_t *results,
                                  uint32_t max_results) {
  search_result_t all_results[MAX_SEARCH_RESULTS];
  uint32_t total_results =
      searchByPrefix(prefix, all_results, MAX_SEARCH_RESULTS);

  uint32_t track_count = 0;
  for (uint32_t i = 0; i < total_results && track_count < max_results; i++) {
    if (all_results[i].type == SEARCH_TRACK) {
      results[track_count++] = all_results[i];
    }
  }

  return track_count;
}

uint32_t MusicIndex::searchGenres(const char *prefix, search_result_t *results,
                                  uint32_t max_results) {
  search_result_t all_results[MAX_SEARCH_RESULTS];
  uint32_t total_results =
      searchByPrefix(prefix, all_results, MAX_SEARCH_RESULTS);

  uint32_t genre_count = 0;
  for (uint32_t i = 0; i < total_results && genre_count < max_results; i++) {
    if (all_results[i].type == SEARCH_GENRE) {
      results[genre_count++] = all_results[i];
    }
  }

  return genre_count;
}

// Relationship functions - placeholders for now
uint32_t MusicIndex::getArtistsByGenre(uint32_t genre_id,
                                       search_result_t *results,
                                       uint32_t max_results) {
  Serial.println("WARNING: getArtistsByGenre not yet implemented");
  return 0;
}

uint32_t MusicIndex::getAlbumsByArtist(uint32_t artist_id,
                                       search_result_t *results,
                                       uint32_t max_results) {
  Serial.println("WARNING: getAlbumsByArtist not yet implemented");
  return 0;
}

uint32_t MusicIndex::getTracksByArtist(uint32_t artist_id,
                                       search_result_t *results,
                                       uint32_t max_results) {
  Serial.println("WARNING: getTracksByArtist not yet implemented");
  return 0;
}

uint32_t MusicIndex::getTracksByAlbum(uint32_t album_id,
                                      search_result_t *results,
                                      uint32_t max_results) {
  Serial.println("WARNING: getTracksByAlbum not yet implemented");
  return 0;
}

void MusicIndex::getResultName(const search_result_t &result, char *buffer,
                               uint32_t buffer_size) {
  readString(result.name_offset, result.name_length, buffer, buffer_size);
}

const char *MusicIndex::getSearchTypeName(SearchType type) {
  switch (type) {
  case SEARCH_TRACK:
    return "Track";
  case SEARCH_ARTIST:
    return "Artist";
  case SEARCH_ALBUM:
    return "Album";
  case SEARCH_GENRE:
    return "Genre";
  default:
    return "Unknown";
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
  
  if (path_index_initialized) {
    Serial.println("=== PATH INDEX STATS ===");
    Serial.print("Track count: ");
    Serial.println(path_header.track_count);
    Serial.print("Path data size: ");
    Serial.print(path_header.path_data_size);
    Serial.println(" bytes");
    Serial.print("Path index base address: 0x");
    Serial.println(path_index_base_address, HEX);
    Serial.print("Track IDs offset: 0x");
    Serial.println(track_ids_offset, HEX);
    Serial.print("Path offsets offset: 0x");
    Serial.println(path_offsets_offset, HEX);
    Serial.print("Path data offset: 0x");
    Serial.println(path_data_offset, HEX);
  } else {
    Serial.println("Path index not initialized");
  }
  
  Serial.println("========================");
}

void MusicIndex::printCacheStats() {
  if (!initialized) {
    Serial.println("Music index not initialized");
    return;
  }

  Serial.println("=== CACHE STATS ===");

  // Count valid node cache entries
  uint32_t valid_nodes = 0;
  for (uint32_t i = 0; i < NODE_CACHE_SIZE; i++) {
    if (node_cache[i].valid) {
      valid_nodes++;
    }
  }

  // Count valid string cache entries
  uint32_t valid_strings = 0;
  for (uint32_t i = 0; i < STRING_CACHE_SIZE; i++) {
    if (string_cache[i].valid) {
      valid_strings++;
    }
  }

  Serial.print("Node cache: ");
  Serial.print(valid_nodes);
  Serial.print("/");
  Serial.print(NODE_CACHE_SIZE);
  Serial.print(" slots used (");
  Serial.print(node_cache_count);
  Serial.println(" indexed)");

  Serial.print("String cache: ");
  Serial.print(valid_strings);
  Serial.print("/");
  Serial.print(STRING_CACHE_SIZE);
  Serial.println(" slots used");

  Serial.print("Access counter: ");
  Serial.println(access_counter);

  Serial.print("Current batch: ");
  if (node_batch.count > 0) {
    Serial.print("nodes ");
    Serial.print(node_batch.start_index);
    Serial.print("-");
    Serial.println(node_batch.start_index + node_batch.count - 1);
  } else {
    Serial.println("empty");
  }

  Serial.println("==================");
}

void MusicIndex::printSearchResult(const search_result_t &result) {
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