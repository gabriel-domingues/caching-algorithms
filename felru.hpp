#include <algorithm>
#include <array>
#include <cinttypes>
#include <deque>
#include <functional>
#include <iostream>
#include <optional>

static const size_t max_entries = (1 << 20) / 27;
static size_t bucket[27] = {0};

struct mul_shift {
  static const uint64_t hi = 0x51502a8334304aae;
  static const uint64_t lo = 0x9743df29cdf1096f;
  uint64_t operator()(uint64_t x) {
    return static_cast<unsigned __int128>(x) * ((static_cast<unsigned __int128>(hi) << 64) | lo) >> 64;
  };
};

template <class pd, typename Hash = std::identity>
struct bin_cache {
  // reference : https://github.com/jbapple/crate-dictionary

  const size_t size = max_entries * 27;
  const size_t entries = max_entries;
  std::vector<pd> pds;
  Hash hasher;

  bin_cache(size_t size) : size(size), entries(size / 27) {
    pds.resize(entries);
  }

  auto set(size_t key, void* val) {
    auto hash = hasher(key);
    auto b = hash % entries;
    uint16_t fp = static_cast<uint16_t>(hash / entries);
    auto& pd_ = pds[b];
    auto lookup = pd_.find(fp);
    auto hit = lookup.has_value();
    if (!hit) pd_.insert(fp, val);
    return hit;
  }

  void describe() {
    std::cout
        << "Cache Eviction Policy: FELRU\n"
        << "Cache size: " << size << std::endl;
  }

  auto buckets() {
    std::array<size_t, 27> buckets;
    std::copy(bucket, bucket + 27, buckets.begin());
    std::fill(bucket, bucket + 27, 0);
    return buckets;
  }
};

namespace bin_dictionary {

struct element {
  uint16_t fp;
  void* val;
  bool operator==(const element& other) const {
    return fp == other.fp;
  }
};
struct bin : std::deque<element> {
  bool operator<(const bin& other) const {
    return size() < other.size();
  }
};
using cache = std::array<bin, 32>;

struct evict_q {
  bin& operator()(cache& bins, uint16_t q) {
    for (uint16_t e = (q + 1) & 31U; e != q; e = (e + 1) & 31U)
      if (!bins[e].empty())
        return bins[e];
    return bins[q];
  }
};

template <typename Evict = evict_q>
struct fe_lru {
  Evict evict;
  void operator()(cache& bins, uint16_t q) {
    auto& victim = evict(bins, q);
    victim.pop_back();
    ++bucket[victim.size()];
  }
};

template <typename Policy = fe_lru<>>
struct pd {
  cache bins;
  size_t occupancy = 0;
  Policy evict;

  std::optional<element> find(uint16_t fp) {
    uint16_t q = fp & 31U;
    uint16_t r = fp >> 5;
    auto& bin_ = bins[q];
    auto slot = std::find(bin_.begin(), bin_.end(), element{r, nullptr});
    if (slot == bin_.end())
      return {};
    else {
      std::rotate(bin_.begin(), slot, slot + 1);
      return bin_.front();
    }
  }

  void insert(uint16_t fp, void* val) {
    uint16_t q = fp & 31U;
    for (; occupancy >= 27; --occupancy) evict(bins, q);
    uint16_t r = fp >> 5;
    bins[q].push_front({r, val});
    ++occupancy;
  }
};

};  // namespace bin_dictionary
