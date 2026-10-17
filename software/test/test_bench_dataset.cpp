// Loads a test bench card's index (opentone bench-dataset, see
// docs/tools/bench.md) into the firmware's lookup and search code, to check
// that the synthetic library fits PSRAM and is searchable before burning a
// card. Skipped unless OPENTONE_BENCH_CARD names the card (or a folder with
// its opentone/ directory):
//   OPENTONE_BENCH_CARD=/media/sd pio test -e test_native -f "*" \
//     -a '--test-case=*bench*'
#include <doctest.h>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../src/state/search.h"
#include "../src/storage/Music_index.h"
#include "../src/storage/Music_lookup.h"
#include "../src/storage/PSRAM_controller.hpp"
#include "test_helpers.h"

namespace {

int loadCardIndex(const std::string &card) {
  std::string dir = card + "/opentone";
  DIR *d = opendir(dir.c_str());
  if (!d)
    return 0;
  int n = 0;
  while (dirent *e = readdir(d)) {
    std::string name = e->d_name;
    bool bin = name.size() > 4 && name.compare(name.size() - 4, 4, ".bin") == 0;
    bool pack = name.size() > 5 && name.compare(name.size() - 5, 5, ".pack") == 0;
    if (!bin && !pack)
      continue;
    std::ifstream f(dir + "/" + name, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
    opentone_test::FakeFs::get().addFile("/opentone/" + name, bytes);
    n++;
  }
  closedir(d);
  return n;
}

void typeQuery(SearchModel &search, MusicLookup &lookup, const char *text) {
  for (const char *p = text; *p; p++) {
    if (*p == ' ') {
      search.space(lookup);
      continue;
    }
    char c = (*p >= 'a' && *p <= 'z') ? *p - 'a' + 'A' : *p;
    int target = (int)(strchr(SearchModel::RIBBON, c) - SearchModel::RIBBON);
    search.moveCursor(target - search.getCursor());
    search.append(lookup);
  }
}

} // namespace

TEST_CASE("Bench dataset - loads and searches") {
  const char *card = getenv("OPENTONE_BENCH_CARD");
  if (!card) {
    MESSAGE("OPENTONE_BENCH_CARD not set, skipped");
    return;
  }
  opentone_test::resetAll();
  REQUIRE(loadCardIndex(card) > 0);

  MusicLookup lookup;
  REQUIRE(lookup.init());
  MESSAGE("tracks=" << lookup.getTrackCount()
                    << " albums=" << lookup.getAlbumCount()
                    << " artists=" << lookup.getArtistCount());
  CHECK(lookup.getTrackCount() > 0);

  // The bench reads the highlighted row's name this way (app/bench.cpp).
  char name[64] = "";
  uint32_t trackId = lookup.getTrackIdAtIndex(0);
  CHECK(lookup.getTrackName(trackId, name, sizeof(name)));
  MESSAGE("track 0: id=" << trackId << " name='" << name << "'");
  uint32_t albumId = lookup.getAlbumIdAtIndex(0);
  name[0] = 0;
  CHECK(lookup.getAlbumName(albumId, name, sizeof(name)));
  MESSAGE("album 0: id=" << albumId << " name='" << name << "'");
  uint32_t artistId = lookup.getArtistIdAtIndex(0);
  name[0] = 0;
  CHECK(lookup.getArtistName(artistId, name, sizeof(name)));
  MESSAGE("artist 0: id=" << artistId << " name='" << name << "'");

  MusicIndex index;
  REQUIRE(index.init(lookup.getSearchTrie()));
  SearchModel search;
  search.setIndex(&index);
  // The runner finds the samples this way (bench/scenarios.ts).
  typeQuery(search, lookup, "opentone bench");
  CHECK(search.getHitCount() > 0);
  const PageStore::Stats &st = pageStore.stats();
  MESSAGE("page cache: " << st.hits << " hits, " << st.misses << " misses");
}
