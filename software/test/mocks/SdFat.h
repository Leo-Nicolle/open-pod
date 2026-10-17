// Minimal SdFat.h mock for native tests.
//
// Replaces the SdFat library with an in-memory file system so the storage
// layer can read its *.bin files without real hardware. Tests populate the
// file system via opentone_test::FakeFs::get().addFile(...).
#pragma once

#include <stdint.h>
#include <string.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifndef O_RDONLY
#define O_RDONLY 0x01
#endif
#ifndef O_RDWR
#define O_RDWR 0x02
#endif

#ifndef SHARED_SPI
#define SHARED_SPI 0
#endif

#define SD_SCK_MHZ(x) (x)

namespace opentone_test {

struct FakeFs {
  std::map<std::string, std::shared_ptr<std::vector<uint8_t>>> files;
  bool sdBeginResult = true;
  // Called with the path before a "*.pack" file is opened, so tests can
  // register index tables one by one and have them packed on demand
  // (test_helpers.h packTables).
  std::function<void(const std::string &)> packer;

  static FakeFs &get() {
    static FakeFs instance;
    return instance;
  }

  void reset() {
    files.clear();
    sdBeginResult = true;
    packer = nullptr;
  }

  void addFile(const std::string &name, const std::vector<uint8_t> &data) {
    files[name] = std::make_shared<std::vector<uint8_t>>(data);
  }
};

} // namespace opentone_test

class SdSpiConfig {
public:
  int cs;
  int mode;
  uint32_t speed;
  SdSpiConfig(int c, int m, uint32_t s) : cs(c), mode(m), speed(s) {}
};

class FsFile {
  std::shared_ptr<std::vector<uint8_t>> data_;
  size_t pos_ = 0;
  bool open_ = false;

public:
  bool open(const char *path, int) {
    open_ = false;
    data_.reset();
    pos_ = 0;
    opentone_test::FakeFs &fs = opentone_test::FakeFs::get();
    std::string p(path);
    if (fs.packer && p.size() > 5 && p.compare(p.size() - 5, 5, ".pack") == 0) {
      fs.packer(p);
    }
    auto &files = fs.files;
    auto it = files.find(p);
    if (it == files.end()) {
      return false;
    }
    data_ = it->second;
    pos_ = 0;
    open_ = true;
    return true;
  }

  operator bool() const { return open_; }
  bool isOpen() const { return open_; }

  size_t size() const { return open_ ? data_->size() : 0; }

  size_t read(void *buf, size_t count) {
    if (!open_ || !buf) {
      return 0;
    }
    size_t remaining = data_->size() - pos_;
    size_t n = (count < remaining) ? count : remaining;
    memcpy(buf, data_->data() + pos_, n);
    pos_ += n;
    return n;
  }

  // The fake card's files are all contiguous.
  bool contiguousRange(uint32_t *bgn, uint32_t *end) {
    if (bgn)
      *bgn = 0;
    if (end)
      *end = 0;
    return open_;
  }

  bool seekSet(uint32_t pos) {
    if (!open_) {
      return false;
    }
    pos_ = (pos > data_->size()) ? data_->size() : pos;
    return true;
  }

  // Overwrites in place, growing the file if needed.
  size_t write(const void *buf, size_t count) {
    if (!open_ || !buf) {
      return 0;
    }
    if (pos_ + count > data_->size()) {
      data_->resize(pos_ + count);
    }
    memcpy(data_->data() + pos_, buf, count);
    pos_ += count;
    return count;
  }

  void close() {
    open_ = false;
    data_.reset();
    pos_ = 0;
  }
};

class SdFat {
public:
  bool begin(const SdSpiConfig &) {
    mounted = opentone_test::FakeFs::get().sdBeginResult;
    return mounted;
  }
  uint8_t fatType() const { return mounted ? 32 : 0; }
  // Opens on this volume, as the real SdFs::open does.
  FsFile open(const char *path, int flags = O_RDONLY) {
    FsFile f;
    f.open(path, flags);
    return f;
  }
  uint32_t sectorsPerCluster() { return 1; }
  uint32_t clusterCount() { return 1024; }

private:
  bool mounted = false;
};

// Real SdFat aliases File to whichever concrete file class the library is
// configured for (FsFile in this project's config); src/sound/Audio_buffer.h
// declares its file handles as `File`, so mirror that alias here.
typedef FsFile File;
