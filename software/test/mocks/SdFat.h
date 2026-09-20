// Minimal SdFat.h mock for native tests.
//
// Replaces the SdFat library with an in-memory file system so the storage
// layer can read its *.bin files without real hardware. Tests populate the
// file system via openpod_test::FakeFs::get().addFile(...).
#pragma once

#include <stdint.h>
#include <string.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#ifndef O_RDONLY
#define O_RDONLY 0x01
#endif

#ifndef SHARED_SPI
#define SHARED_SPI 0
#endif

#define SD_SCK_MHZ(x) (x)

namespace openpod_test {

struct FakeFs {
  std::map<std::string, std::shared_ptr<std::vector<uint8_t>>> files;
  bool sdBeginResult = true;

  static FakeFs &get() {
    static FakeFs instance;
    return instance;
  }

  void reset() {
    files.clear();
    sdBeginResult = true;
  }

  void addFile(const std::string &name, const std::vector<uint8_t> &data) {
    files[name] = std::make_shared<std::vector<uint8_t>>(data);
  }
};

} // namespace openpod_test

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
    auto &files = openpod_test::FakeFs::get().files;
    auto it = files.find(std::string(path));
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

  bool seekSet(uint32_t pos) {
    if (!open_) {
      return false;
    }
    pos_ = (pos > data_->size()) ? data_->size() : pos;
    return true;
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
    return openpod_test::FakeFs::get().sdBeginResult;
  }
  uint32_t sectorsPerCluster() { return 1; }
  uint32_t clusterCount() { return 1024; }
};

// Real SdFat aliases File to whichever concrete file class the library is
// configured for (FsFile in this project's config); src/sound/Audio_buffer.h
// declares its file handles as `File`, so mirror that alias here.
typedef FsFile File;
