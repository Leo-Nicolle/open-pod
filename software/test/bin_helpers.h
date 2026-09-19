// Helpers to build the binary index/relation files that the indexer produces,
// so tests can exercise MusicLookup / MusicIndex against known data.
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

// Mirrors indexer exportStringIndexToBinary:
// [entry_count][string_data_size][ids...][offsets...][string_data...]
inline std::vector<uint8_t>
buildStringIndex(const std::vector<std::pair<uint32_t, std::string>> &entries) {
  std::vector<std::pair<uint32_t, std::string>> sorted = entries;
  std::sort(sorted.begin(), sorted.end(),
            [](const std::pair<uint32_t, std::string> &a,
               const std::pair<uint32_t, std::string> &b) {
              return a.first < b.first;
            });

  std::vector<uint8_t> out;
  std::vector<uint32_t> ids;
  std::vector<uint32_t> offsets;
  std::vector<uint8_t> strings;
  uint32_t stringsLength = 0;

  offsets.push_back(0);
  for (size_t i = 0; i < sorted.size(); i++) {
    ids.push_back(sorted[i].first);
    for (size_t j = 0; j < sorted[i].second.size(); j++) {
      strings.push_back((uint8_t)sorted[i].second[j]);
    }
    strings.push_back(0); // null terminator
    stringsLength += (uint32_t)sorted[i].second.size() + 1;
    offsets.push_back(stringsLength);
  }

  pushU32(out, (uint32_t)ids.size()); // entry count
  pushU32(out, stringsLength);        // string data size
  for (size_t i = 0; i < ids.size(); i++)
    pushU32(out, ids[i]);
  for (size_t i = 0; i < offsets.size(); i++)
    pushU32(out, offsets[i]);
  out.insert(out.end(), strings.begin(), strings.end());
  return out;
}

// Mirrors indexer exportRelationshipMapToBinary:
// [entry_count][total_target_count][source_ids...][target_counts...][target_ids...]
inline std::vector<uint8_t> buildRelation(
    const std::vector<std::pair<uint32_t, std::vector<uint32_t>>> &map) {
  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> sorted = map;
  std::sort(sorted.begin(), sorted.end(),
            [](const std::pair<uint32_t, std::vector<uint32_t>> &a,
               const std::pair<uint32_t, std::vector<uint32_t>> &b) {
              return a.first < b.first;
            });

  std::vector<uint8_t> out;
  std::vector<uint32_t> sourceIds;
  std::vector<uint32_t> targetCounts;
  std::vector<uint32_t> targetIds;

  for (size_t i = 0; i < sorted.size(); i++) {
    sourceIds.push_back(sorted[i].first);
    std::vector<uint32_t> targets = sorted[i].second;
    std::sort(targets.begin(), targets.end());
    targetCounts.push_back((uint32_t)targets.size());
    for (size_t j = 0; j < targets.size(); j++)
      targetIds.push_back(targets[j]);
  }

  pushU32(out, (uint32_t)sourceIds.size()); // entry count
  pushU32(out, (uint32_t)targetIds.size()); // total target count
  for (size_t i = 0; i < sourceIds.size(); i++)
    pushU32(out, sourceIds[i]);
  for (size_t i = 0; i < targetCounts.size(); i++)
    pushU32(out, targetCounts[i]);
  for (size_t i = 0; i < targetIds.size(); i++)
    pushU32(out, targetIds[i]);
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

} // namespace openpod_test
