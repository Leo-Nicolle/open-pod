#include "Music_index.h"
#include "../logging.h"

// MusicIndex node/string LRU caches in front of the PSRAM-resident trie. The
// search itself is in Music_index.cpp.

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
    psram.readData(addr, (uint8_t *)&single_node, sizeof(trie_node_t));
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

void MusicIndex::printCacheStats() {
  if (!initialized) {
    LOG_WARN("Music index not initialized");
    return;
  }


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

  LOG_INFO("=== CACHE STATS ===\n"
           "Node cache: %lu/%lu slots used (%lu indexed)\n"
           "String cache: %lu/%lu slots used\n"
           "Access counter: %lu",
           (unsigned long)valid_nodes, (unsigned long)NODE_CACHE_SIZE,
           (unsigned long)node_cache_count, (unsigned long)valid_strings,
           (unsigned long)STRING_CACHE_SIZE, (unsigned long)access_counter);
  if (node_batch.count > 0) {
    LOG_INFO("Current batch: nodes %lu-%lu",
             (unsigned long)node_batch.start_index,
             (unsigned long)(node_batch.start_index + node_batch.count - 1));
  } else {
    LOG_INFO("Current batch: empty");
  }
  LOG_INFO("==================");
}
