# Music Index Generator

This tool generates a binary search index from your music collection that can be loaded by the STM32 Music_index class.

## Compatibility

The generated binary files are fully compatible with the C++ `MusicIndex` class in `software/src/storage/Music_index.hpp`. The binary format includes:

1. **Header** (16 bytes): `trie_header_t` with counts and sizes
2. **String Pool**: All text data (track names, artists, etc.) concatenated
3. **String Offsets**: Array of offsets into the string pool (4-byte aligned)
4. **Nodes**: Trie node structures for navigation
5. **Results**: Search result structures with metadata

## Usage

### Generate Index from Music Directory

````bash
# Generate from your music collection
npm run generate /path/to/your/music ./music_index.bin

### Visualise it

```bash
hexdump -C music_index.bin | head -10
````

### Use in STM32

1. Copy the generated `music_index.bin` to your SD card root directory
2. Initialize in your STM32 code:

```cpp
#include "storage/Music_index.hpp"

MusicIndex musicIndex(&psram_controller);
if (musicIndex.init("/music_index.bin")) {
    // Search for tracks, artists, albums, genres
    search_result_t results[20];
    uint32_t count = musicIndex.searchByPrefix("rock", results, 20);

    for (uint32_t i = 0; i < count; i++) {
        char name[128];
        musicIndex.getResultName(results[i], name, sizeof(name));
        Serial.printf("[%s] %s (relevance: %d)\n",
                     musicIndex.getSearchTypeName((SearchType)results[i].type),
                     name, results[i].relevance);
    }
}
```

## Binary Format Details

The binary format is designed for efficient loading into PSRAM and fast searching:

- **Little-endian** byte order (matches STM32)
- **4-byte aligned** structures for optimal memory access
- **Compact string storage** with offset-based references
- **Hierarchical trie structure** for prefix-based searching

### Data Structures

```c
typedef struct {
    uint32_t string_pool_size;
    uint32_t node_count;
    uint32_t result_count;
    uint32_t string_offset_count;
} trie_header_t;

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
```

## Search Features

- **Prefix matching**: Find all items starting with a given prefix
- **Partial word matching**: Match individual words within names
- **Relevance scoring**: Results ranked by match quality
- **Type filtering**: Search specific types (tracks, artists, albums, genres)
- **Case-insensitive**: Normalized search terms

## File Outputs

- `music_index.bin`: Binary data file for STM32 (copy to SD card)
