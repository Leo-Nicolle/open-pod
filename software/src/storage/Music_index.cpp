#include "Music_index.h"
#include "SharedSpiDriver.h"
#include "../sound/AudioYield.h"
#include "../logging.h"

MusicIndex::MusicIndex( uint32_t psram_base_addr)
    : base_address(psram_base_addr),
      initialized(false), node_cache_count(0), access_counter(0) {
  memset(&header, 0, sizeof(header));
  memset(&node_batch, 0, sizeof(node_batch));
  initCache();
}

MusicIndex::~MusicIndex() {
  // Nothing to clean up - PSRAM controller is managed externally
}

bool MusicIndex::init(uint32_t base_address) {
  this->base_address = base_address;
  LOG_INFO("=== MUSIC INDEX INIT ===");
  LOG_CAT(INDEX_USAGE, "Loading index from: %s", index_filename);

  // The index lives in PSRAM: make sure it's up (idempotent) rather than
  // relying on another subsystem having initialized it first.
  if (!psram.init()) {
    LOG_ERROR("PSRAM initialization failed");
    return false;
  }

  // Initialize SD card with default SPI configuration
  if (!sd.begin(OPENPOD_SD_CONFIG(CARDCS))) {
    LOG_ERROR("SD card initialization failed");
    return false;
  }

  // Load the binary trie data from SD card to PSRAM
  if (!loadFromSDCard(index_filename)) {
    LOG_ERROR("Failed to load music index from SD card");
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

  LOG_INFO("Music index loaded successfully!");
  // printStats();

  return true;
}

bool MusicIndex::initWithSDConfig(SdSpiConfig sdConfig, const char *index_filename) {
  LOG_INFO("=== MUSIC INDEX INIT WITH CUSTOM CONFIG ===");
  LOG_CAT(INDEX_USAGE, "Loading index from: %s", index_filename);

  // The index lives in PSRAM: make sure it's up (idempotent) rather than
  // relying on another subsystem having initialized it first.
  if (!psram.init()) {
    LOG_ERROR("PSRAM initialization failed");
    return false;
  }

  // Initialize SD card with custom configuration
  if (!sd.begin(sdConfig)) {
    LOG_ERROR("SD card initialization failed with custom config");
    return false;
  }

  // Load the binary trie data from SD card to PSRAM
  if (!loadFromSDCard(index_filename)) {
    LOG_ERROR("Failed to load music index from SD card");
    return false;
  }

  // Calculate offsets within the loaded data
  string_pool_offset = base_address + sizeof(trie_header_t);
  uint32_t string_pool_padded = (header.string_pool_size);
  string_offsets_offset = string_pool_offset + string_pool_padded;
  nodes_offset = string_offsets_offset + (header.string_offset_count * 4);
  results_offset = nodes_offset + (header.node_count * sizeof(trie_node_t));

  initialized = true;
  clearCache();

  LOG_INFO("Music index loaded successfully with custom config!");
  printStats();

  return true;
}

uint32_t MusicIndex::readLittleEndian32(FsFile &file) {
  uint8_t bytes[4];
  file.read(bytes, 4);
  return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
         ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

bool MusicIndex::loadFromSDCard(const char *filename) {
  // Open file using SDFat
  FsFile file;
  if (!file.open(filename, O_RDONLY)) {
    LOG_ERROR("Could not open file: %s", filename);
    return false;
  }

  uint32_t file_size = file.size();
  LOG_CAT(INDEX_USAGE, "File size: %lu bytes", (unsigned long)file_size);

  if (file.read(&header, sizeof(header)) != sizeof(header)) {
    LOG_ERROR("Failed to read header");
    file.close();
    return false;
  }

  LOG_CAT(INDEX_USAGE,
          "Header - String pool: %lu, Nodes: %lu, Results: %lu, "
          "String offsets: %lu",
          (unsigned long)header.string_pool_size,
          (unsigned long)header.node_count, (unsigned long)header.result_count,
          (unsigned long)header.string_offset_count);

  // Reset file position to beginning
  file.seekSet(0);

  // One SD sector at a time, on the stack: SdFat reads whole aligned sectors
  // straight into it, and there's no heap left for a bigger one (the list
  // cache takes most of it).
  const uint32_t CHUNK_SIZE = 512;
  uint8_t buffer[CHUNK_SIZE];

  uint32_t bytes_read = 0;
  uint32_t psram_addr = base_address;

  while (bytes_read < file_size) {
    uint32_t to_read = min(CHUNK_SIZE, file_size - bytes_read);
    uint32_t actual_read = file.read(buffer, to_read);

    if (actual_read == 0) {
      LOG_ERROR("Failed to read from file");
      file.close();
      return false;
    }

    psram.writeData(psram_addr, buffer, actual_read);
    psram_addr += actual_read;
    bytes_read += actual_read;

    // Show progress every 16KB
    if (bytes_read % 16384 == 0) {
      LOG_CAT(INDEX_USAGE, "Loaded %lu/%lu bytes", (unsigned long)bytes_read,
              (unsigned long)file_size);
    }
  }

  file.close();

  LOG_CAT(INDEX_USAGE, "Successfully loaded %lu bytes to PSRAM",
          (unsigned long)bytes_read);

  return true;
}

void MusicIndex::readString(uint32_t offset, uint32_t length, char *buffer,
                             uint32_t buffer_size) {
  uint32_t copy_len = min(length, buffer_size - 1);
  psram.readData(string_pool_offset + offset, (uint8_t *)buffer, copy_len);
  buffer[copy_len] = '\0';
}

void MusicIndex::loadNodeBatch(uint32_t start_index, uint32_t count) {
  // Limit count to batch size
  count = min(count, (uint32_t)NODE_BATCH_SIZE);

  // Update batch info
  node_batch.start_index = start_index;
  node_batch.count = count;

  // Read all nodes in one PSRAM operation
  uint32_t addr = nodes_offset + (start_index * sizeof(trie_node_t));
  psram.readData(addr, (uint8_t *)node_batch.nodes,
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
  psram.readData(addr, (uint8_t *)results, count * sizeof(search_result_t));
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
      if ((i & 7) == 7) {
        audioYield(); // the root has a child per first letter/word: many reads
      }
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

  // A short prefix's subtree spans hundreds of nodes: keep the audio fifo
  // topped up while walking it.
  audioYield();
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
    LOG_WARN("Music index not initialized");
    return;
  }

  uint64_t sectors = sd.sectorsPerCluster() * sd.clusterCount();

  // Each sector is 512 bytes. No %f in nano printf: GB with one decimal.
  uint64_t cardSizeBytes = sectors * 512ULL;
  unsigned long cardSizeMB = (unsigned long)(cardSizeBytes / (1024ULL * 1024ULL));
  unsigned long cardSizeGB10 = cardSizeMB * 10 / 1024;
  LOG_INFO("=== MUSIC INDEX STATS ===\n"
           "String pool size: %lu bytes\n"
           "Node count: %lu\n"
           "Result count: %lu\n"
           "String offset count: %lu\n"
           "Base address: 0x%lX\n"
           "String pool offset: 0x%lX\n"
           "Nodes offset: 0x%lX\n"
           "Results offset: 0x%lX\n"
           "SD card size: %lu MB (%lu.%lu GB)\n"
           "========================",
           (unsigned long)header.string_pool_size,
           (unsigned long)header.node_count, (unsigned long)header.result_count,
           (unsigned long)header.string_offset_count,
           (unsigned long)base_address, (unsigned long)string_pool_offset,
           (unsigned long)nodes_offset, (unsigned long)results_offset,
           cardSizeMB, cardSizeGB10 / 10, cardSizeGB10 % 10);
}

void MusicIndex::printSearchResult(const search_result_t &result) {
  char name[128];
  getResultName(result, name, sizeof(name));

  LOG_INFO("[%s] %s (ID: %lu, Relevance: %lu)",
           getSearchTypeName((SearchType)result.type), name,
           (unsigned long)result.id, (unsigned long)result.relevance);
}
