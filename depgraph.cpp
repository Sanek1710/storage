#include <alloca.h>
#include "allocator.h"

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <string_view>
#include <vector>

#include "dense.h"
#include "fmt/format.h"
#include "fmt/ranges.h"
#include "spdlog/stopwatch.h"

using FileID = unsigned;

// using DynamicBitSet = dense::set<unsigned>;

class DynamicBitSet {
  static constexpr size_t initialSize = 1024Ull * 1024;

 public:
  bool contains(unsigned i) const { return bitset.size() > i && bitset[i]; }
  void insert(unsigned i) {
    if (bitset.size() <= i) bitset.resize(i + 1);
    bitset[i] = true;
  }

 private:
  std::vector<bool> bitset = std::vector<bool>(initialSize);
};

class FileSuffixIndex {
  template <typename V>
  using string_view_segmented_map =
      dense::segmented_map<std::string_view, V, dense::string_hash,
                           std::equal_to<>>;

 public:
  struct StableCursor {
    StableCursor() = default;

    const std::vector<FileID>& operator*() { return *ptr; }
    const std::vector<FileID>* operator->() { return ptr; }

    bool operator==(StableCursor other) const { return ptr == other.ptr; }
    bool operator!=(StableCursor other) const { return ptr != other.ptr; }

    operator bool() const { return ptr; }

   private:
    const std::vector<FileID>* ptr = nullptr;
    StableCursor(const std::vector<FileID>* ptr) : ptr(ptr) {}
    friend FileSuffixIndex;
  };

  void insert(std::string_view path, FileID id) {
    std::string_view suffix = *pathStorage.emplace(path).first;
    while (!suffix.empty()) {
      fileSfxGroup[suffix].push_back(id);
      const size_t pos = suffix.find('/');
      if (pos == std::string_view::npos) break;
      suffix.remove_prefix(pos + 1);
    }
  }

  StableCursor prepare(std::string_view sfx) {
    do {
      if (sfx.empty()) return end();
      if (sfx.rfind("./", 0) == 0) {
        sfx.remove_prefix(2);
      } else if (sfx.rfind("../", 0) == 0) {
        sfx.remove_prefix(3);
      } else {
        break;
      }
    } while (true);
    if (auto sfxIt = fileSfxGroup.find(sfx); sfxIt != fileSfxGroup.end()) {
      return &sfxIt->second;
    }
    std::string_view suffix = *preparedSfxStorage.emplace(sfx).first;
    return &fileSfxGroup[suffix];
  }

  StableCursor find(std::string_view sfx) const {
    auto sfxIt = fileSfxGroup.find(sfx);
    if (sfxIt == fileSfxGroup.end()) return end();
    return &sfxIt->second;
  }
  StableCursor end() const { return StableCursor{}; }

  void print(std::ostream&) const;
  void stats(std::ostream&) const;

 private:
  dense::string_segmented_set pathStorage;
  dense::string_segmented_set preparedSfxStorage;
  string_view_segmented_map<std::vector<FileID>> fileSfxGroup;
};

void FileSuffixIndex::print(std::ostream& os) const {
  for (const auto& [sfx, fileids] : fileSfxGroup) {
    os << fmt::format("{}: {::}\n", sfx, fileids);
  }
};
void FileSuffixIndex::stats(std::ostream& os) const {
  os << fmt::format("       paths: {}\n", pathStorage.size());
  os << fmt::format("prepared sfx: {}\n", preparedSfxStorage.size());
  os << fmt::format("  sfx groups: {}\n", fileSfxGroup.size());
};

auto read_files() {
  std::ifstream ifs{"/home/user/playground/supertrie/.tests/files-big.txt"};
  std::string str;
  std::vector<std::string> files;
  while (ifs >> str) files.push_back(std::move(str));
  return files;
}
auto read_deps() {
  std::ifstream ifs{"/home/user/playground/supertrie/.tests/deps-big.txt"};
  FileID id;
  std::string str;
  std::vector<std::pair<FileID, std::string>> deps;
  while (ifs >> id >> std::quoted(str)) deps.emplace_back(id, std::move(str));
  return deps;
}

class Depgraph {
 public:
  void add_file(std::string_view path, FileID fileId) {
    fsi.insert(path, fileId);
    if (depgraph.size() <= fileId) depgraph.resize(fileId + 1);
  }
  void add_dep(FileID fileId, std::string_view dep) {
    if (depgraph.size() <= fileId) return;
    depgraph[fileId].push_back(fsi.prepare(dep));
  }

  FileID size() const { return depgraph.size(); }

  auto closure(FileID fileId, DynamicBitSet& visited) const {
    auto dfs = [this, &visited](auto self, FileID fileId) -> void {
      for (auto cursor : depgraph[fileId]) {
        for (auto depfileId : *cursor) {
          if (visited.contains(depfileId)) continue;
          visited.insert(depfileId);
          self(self, depfileId);
        }
      }
    };
    visited.insert(fileId);
    dfs(dfs, fileId);
  }

  auto closure(FileID fileId) {
    DynamicBitSet visited;
    closure(fileId, visited);
    return visited;
  }

 private:
  std::vector<std::vector<FileSuffixIndex::StableCursor>> depgraph;
  FileSuffixIndex fsi;
};

int main(int argc, char* argv[]) {
  Depgraph depgraph;
  {
    auto files = read_files();
    spdlog::stopwatch sw;
    FileID fileId = 0;
    for (const auto file : files) {
      depgraph.add_file(file, fileId++);
    }
    fmt::print(" insered in: {} s\n", sw);
  }

  {
    auto deps = read_deps();
    spdlog::stopwatch sw;
    for (const auto& [id, dep] : deps) {
      depgraph.add_dep(id, dep);
    }
    fmt::print("prepared in: {} s\n", sw);
  }

  size_t total = 0;
  spdlog::stopwatch sw;
  for (unsigned fileId = 0; fileId < depgraph.size(); ++fileId) {
    auto visited = depgraph.closure(fileId);
    for (int j = 0; j < 10; ++j) {
      total += visited.contains(j);
    }
  }
  fmt::print("dfsed in: {} s\n", sw);
  fmt::print("total: {}\n", total);
  fmt::print("allocated: {}\n", allocated);

  return 0;
}
