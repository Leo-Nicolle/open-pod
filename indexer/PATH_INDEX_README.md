# Path Index for Track File Retrieval

This document explains the new path index functionality that allows you to retrieve file paths for tracks after searching with the trie.

## Overview

The path index is a separate binary file that provides fast lookup of file paths by track ID. This keeps the main trie memory-efficient while enabling path retrieval when users select tracks.

## Architecture

```
User Search → Trie Search → Track Results (with IDs) → Path Lookup → File Paths
                ↓                                        ↑
        music_index.bin                          music_index_paths.bin
```

## Files Generated

When you run the indexer, it now generates two files:

1. **`music_index.bin`** - The main trie for searching (unchanged)
2. **`music_index_paths.bin`** - The path index for track path lookup (new)

## Usage

### 1. Generation

```bash
# Generate both trie and path index
tsx generate.ts /path/to/music ./music_index.bin

# This creates:
# - music_index.bin (trie)
# - music_index_paths.bin (path index)
```

### 2. TypeScript/Node.js Usage

```typescript
import {
  buildTrieFromCrawlIndex,
  buildPathIndexFromCrawlIndex,
  serializeTrie,
  serializePathIndex,
  exportToBinary,
  exportPathIndexToBinary,
  searchTrie,
  lookupTrackPath,
} from "./src/trie";

// Build both indexes
const trie = buildTrieFromCrawlIndex(crawlIndex);
const pathIndex = buildPathIndexFromCrawlIndex(crawlIndex);

// Search for tracks
const results = searchTrie(trie, "hotel california");

// Get path for selected track
const trackId = results[0].id; // User selects first result
const serializedPathIndex = serializePathIndex(pathIndex);
const filePath = lookupTrackPath(serializedPathIndex, trackId);
console.log(`Play: ${filePath}`);
```

### 3. STM32 Integration

You'll need to update your STM32 code to load and use the path index:

#### Header Updates (Music_index.h)

```cpp
// Add path index structures
typedef struct {
    uint32_t track_count;
    uint32_t path_data_size;
} path_index_header_t;

class MusicIndex {
private:
    // Add path index offsets
    uint32_t path_index_base_address;
    uint32_t path_data_offset;
    uint32_t path_offsets_offset;
    uint32_t track_ids_offset;
    path_index_header_t path_header;

public:
    // Add path lookup method
    bool getTrackPath(uint32_t track_id, char* buffer, uint32_t buffer_size);
    bool initPathIndex(const char* path_index_filename = "/music_index_paths.bin");
};
```

#### Implementation Updates (Music_index.cpp)

```cpp
bool MusicIndex::initPathIndex(const char* path_index_filename) {
    // Load path index binary file to PSRAM
    // Similar to existing init() but for path index
    // Calculate offsets for track IDs, path offsets, and path data
}

bool MusicIndex::getTrackPath(uint32_t track_id, char* buffer, uint32_t buffer_size) {
    // Binary search in track_ids array
    // Use found index to get path offset
    // Read path string from path data
    // Return path in buffer
}
```

## Binary Format

### Path Index Binary Structure

```
Header (8 bytes):
  - track_count (4 bytes)
  - path_data_size (4 bytes)

Track IDs (track_count * 4 bytes):
  - Sorted array of track IDs for binary search

Path Offsets (track_count * 4 bytes):
  - Offset of each path in path_data

Path Data (path_data_size bytes):
  - Concatenated null-terminated path strings
```

## Performance

- **Path Lookup**: O(log n) binary search by track ID
- **Memory Usage**: Only loaded when needed
- **Storage**: Separate from trie, doesn't affect search performance

## Example Workflow

1. **User searches**: "hotel" → Gets list of tracks with IDs
2. **User selects**: Track ID 42 "Hotel California"
3. **System looks up path**: `getTrackPath(42)` → `/music/Eagles/Hotel California/01 - Hotel California.mp3`
4. **System plays file**: Opens and plays the file

## Testing

Run the test suite to verify functionality:

```bash
cd indexer
npm test path-index.test.ts
```

Run the example to see it in action:

```bash
tsx example-usage.ts
```

## File Sizes

Typical sizes for a 10,000 track library:

- **Trie**: ~2-5 MB (depends on metadata complexity)
- **Path Index**: ~500KB - 2MB (depends on path lengths)

The path index is much smaller than including paths in the trie itself.

## Integration Checklist

- [ ] Update STM32 code to load path index file
- [ ] Implement binary search for track ID lookup
- [ ] Add path extraction from concatenated string data
- [ ] Test with real music collection
- [ ] Copy both `.bin` files to SD card
- [ ] Verify search → select → play workflow

## Troubleshooting

**Path not found**: Check that track ID exists in path index
**Binary format errors**: Ensure both files are from same generation run
**Memory issues**: Path index uses minimal RAM, only during lookup
**Performance**: Path lookup should be <1ms on STM32
