// Helpers to build the index tables the syncer produces (as sections of
// index.pack, see syncer/crates/core/src/index/binary.rs and pack.rs), so
// tests can exercise MusicLookup / MusicIndex against known data.
#pragma once

#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace openpod_test {

inline void pushU32(std::vector<uint8_t> &out, uint32_t v) {
  out.push_back((uint8_t)(v & 0xFF));
  out.push_back((uint8_t)((v >> 8) & 0xFF));
  out.push_back((uint8_t)((v >> 16) & 0xFF));
  out.push_back((uint8_t)((v >> 24) & 0xFF));
}

// Mirrors syncer binary::string_index (ids are dense: entry i is id i,
// missing ids up to the highest one are ""):
// [entry_count][string_data_size][offsets...(count+1)][string_data...]
inline std::vector<uint8_t>
buildStringIndex(const std::vector<std::pair<uint32_t, std::string>> &entries) {
  uint32_t count = 0;
  for (size_t i = 0; i < entries.size(); i++)
    count = std::max(count, entries[i].first + 1);
  std::vector<std::string> names(count);
  for (size_t i = 0; i < entries.size(); i++)
    names[entries[i].first] = entries[i].second;

  std::vector<uint8_t> out;
  std::vector<uint8_t> strings;
  std::vector<uint32_t> offsets(1, 0);
  for (size_t i = 0; i < names.size(); i++) {
    strings.insert(strings.end(), names[i].begin(), names[i].end());
    strings.push_back(0); // null terminator
    offsets.push_back((uint32_t)strings.size());
  }
  pushU32(out, count);                    // entry count
  pushU32(out, (uint32_t)strings.size()); // string data size
  for (size_t i = 0; i < offsets.size(); i++)
    pushU32(out, offsets[i]);
  out.insert(out.end(), strings.begin(), strings.end());
  return out;
}

// Mirrors syncer binary::relation: every source id below the highest one
// (or `sources`) has an entry, empty ones too:
// [source_count][total_target_count][start...(count+1)][target_ids...]
inline std::vector<uint8_t> buildRelation(
    const std::vector<std::pair<uint32_t, std::vector<uint32_t>>> &map,
    uint32_t sources = 0) {
  uint32_t count = sources;
  for (size_t i = 0; i < map.size(); i++)
    count = std::max(count, map[i].first + 1);
  std::vector<std::vector<uint32_t>> targets(count);
  for (size_t i = 0; i < map.size(); i++) {
    targets[map[i].first] = map[i].second;
    std::sort(targets[map[i].first].begin(), targets[map[i].first].end());
  }
  std::vector<uint8_t> out;
  std::vector<uint32_t> starts(1, 0);
  std::vector<uint32_t> ids;
  for (size_t i = 0; i < targets.size(); i++) {
    ids.insert(ids.end(), targets[i].begin(), targets[i].end());
    starts.push_back((uint32_t)ids.size());
  }
  pushU32(out, count);
  pushU32(out, (uint32_t)ids.size());
  for (size_t i = 0; i < starts.size(); i++)
    pushU32(out, starts[i]);
  for (size_t i = 0; i < ids.size(); i++)
    pushU32(out, ids[i]);
  return out;
}

// Mirrors syncer binary::letter_table: where each `letters`-letter bucket of
// an alphabetical list starts, [count]{u32 start, u8 key[4]}*count. `keyOf`
// is fastScrollKey (passed in, so this header needs no firmware code).
inline std::vector<uint8_t>
buildLetterTable(const std::vector<std::string> &names, int letters,
                 void (*keyOf)(const char *, int, char *)) {
  std::vector<uint8_t> out;
  std::vector<std::pair<uint32_t, std::string>> entries;
  for (size_t i = 0; i < names.size(); i++) {
    char k[4];
    keyOf(names[i].c_str(), letters, k);
    if (entries.empty() || entries.back().second != k)
      entries.push_back(std::make_pair((uint32_t)i, std::string(k)));
  }
  pushU32(out, (uint32_t)entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    pushU32(out, entries[i].first);
    for (size_t c = 0; c < 4; c++)
      out.push_back(c < entries[i].second.size() ? (uint8_t)entries[i].second[c] : 0);
  }
  return out;
}

// Mirrors syncer index::pack::pack: "OPIX" | u16 2 | u16 0 | u32 count |
// {u32 id, u32 offset, u32 length}*count, sections 16-byte aligned.
inline std::vector<uint8_t>
buildPack(const std::vector<std::pair<uint32_t, std::vector<uint8_t>>> &sections) {
  const size_t ALIGN = 16;
  auto align = [&](size_t n) { return (n + ALIGN - 1) / ALIGN * ALIGN; };
  std::vector<uint8_t> out;
  out.push_back('O');
  out.push_back('P');
  out.push_back('I');
  out.push_back('X');
  out.push_back(2);
  out.push_back(0);
  out.push_back(0);
  out.push_back(0);
  pushU32(out, (uint32_t)sections.size());
  size_t offset = align(12 + sections.size() * 12);
  std::vector<size_t> offsets;
  for (size_t i = 0; i < sections.size(); i++) {
    pushU32(out, sections[i].first);
    pushU32(out, (uint32_t)offset);
    pushU32(out, (uint32_t)sections[i].second.size());
    offsets.push_back(offset);
    offset = align(offset + sections[i].second.size());
  }
  for (size_t i = 0; i < sections.size(); i++) {
    out.resize(offsets[i], 0);
    out.insert(out.end(), sections[i].second.begin(), sections[i].second.end());
  }
  out.resize(align(out.size()), 0);
  return out;
}

// Mirrors indexer exportDurationIndexToBinary:
// [entry_count][uint16 seconds]*entry_count, directly indexed by track id
// (gaps up to the highest id default to 0, same as the indexer).
inline std::vector<uint8_t>
buildDurationIndex(const std::vector<std::pair<uint32_t, uint16_t>> &entries) {
  uint32_t maxId = 0;
  bool any = false;
  for (size_t i = 0; i < entries.size(); i++) {
    if (!any || entries[i].first > maxId) {
      maxId = entries[i].first;
    }
    any = true;
  }
  uint32_t entryCount = any ? maxId + 1 : 0;

  std::vector<uint16_t> durations(entryCount, 0);
  for (size_t i = 0; i < entries.size(); i++) {
    durations[entries[i].first] = entries[i].second;
  }

  std::vector<uint8_t> out;
  pushU32(out, entryCount);
  for (size_t i = 0; i < durations.size(); i++) {
    out.push_back((uint8_t)(durations[i] & 0xFF));
    out.push_back((uint8_t)((durations[i] >> 8) & 0xFF));
  }
  return out;
}

// Mirrors indexer exportTrackToAlbumIndexToBinary:
// [entry_count][uint32 albumId]*entry_count, directly indexed by track id
// (gaps up to the highest id default to 0, same as the indexer).
inline std::vector<uint8_t> buildTrackToAlbumIndex(
    const std::vector<std::pair<uint32_t, uint32_t>> &entries) {
  uint32_t maxId = 0;
  bool any = false;
  for (size_t i = 0; i < entries.size(); i++) {
    if (!any || entries[i].first > maxId) {
      maxId = entries[i].first;
    }
    any = true;
  }
  uint32_t entryCount = any ? maxId + 1 : 0;

  std::vector<uint32_t> albumIds(entryCount, 0);
  for (size_t i = 0; i < entries.size(); i++) {
    albumIds[entries[i].first] = entries[i].second;
  }

  std::vector<uint8_t> out;
  pushU32(out, entryCount);
  for (size_t i = 0; i < albumIds.size(); i++)
    pushU32(out, albumIds[i]);
  return out;
}

struct CoverIndexEntry {
  uint32_t albumId;
  uint32_t offset;
  uint32_t length;
  uint8_t format;
};

// Mirrors indexer exportAlbumCoverIndexToBinary:
// [entry_count][{u32 offset, u32 length, u8 format}]*entry_count, directly
// indexed by album id (gaps up to the highest id default to
// {offset:0, length:0, format:0}, same as the indexer).
inline std::vector<uint8_t>
buildAlbumCoverIndex(const std::vector<CoverIndexEntry> &entries) {
  uint32_t maxId = 0;
  bool any = false;
  for (size_t i = 0; i < entries.size(); i++) {
    if (!any || entries[i].albumId > maxId) {
      maxId = entries[i].albumId;
    }
    any = true;
  }
  uint32_t entryCount = any ? maxId + 1 : 0;

  std::vector<CoverIndexEntry> dense(entryCount, {0, 0, 0, 0});
  for (size_t i = 0; i < entries.size(); i++) {
    dense[entries[i].albumId] = entries[i];
  }

  std::vector<uint8_t> out;
  pushU32(out, entryCount);
  for (size_t i = 0; i < dense.size(); i++) {
    pushU32(out, dense[i].offset);
    pushU32(out, dense[i].length);
    out.push_back(dense[i].format);
  }
  return out;
}

} // namespace openpod_test
