#ifndef MREDIS_SRC_DICT_H_
#define MREDIS_SRC_DICT_H_

#include <cstdint>
#include <functional>
#include <vector>
#include <utility>
#include <limits>

namespace {
  const size_t kTableInitSize = 4;
  const size_t kLongMax = std::numeric_limits<long>::max();
  const size_t kCanResizeRatio = 1;
  const size_t kForceResizeRatio = 5;
  bool can_resize = true;
  uint32_t hash_seed = 5381;
}

namespace mredis {

template<typename TKey, typename TValue>
class Dictionary {
 private:
  struct DictEntry {
    TKey key;
    TValue value;
    DictEntry* next;
  };
  
  struct DictTable {
    // array of DictEntry*
    DictEntry** table;
    size_t size;
    size_t sizemask;
    size_t used;
    void Reset() {
      table = nullptr;
      size = 0;
      sizemask = 0;
      used = 0;
    }
  };

  DictTable dict_[2];
  long rehashidx_;
  int iterator_count_;

  std::function<size_t(const TKey& key)> hash_func_;

 public:
  Dicionary(std::function<size_t(const TKey&)> hash_func) : hash_func_(hash_func) {}
  ~Dictionary();
  bool Expand(size_t size);
  bool Shrink();
  bool Insert(TKey key, TValue value);
  bool Replace(TKey key, TValue value);
  bool Erase(TKey key);
  bool TryFetch(const TKey& key, TValue* value);
  bool TryFetchRandom(TKey* key, TValue* value);
  std::vector<std::pair<TKey*, TValue*>> FetchSome(int count);
  void Clear();
  size_t RehashMilliseconds(int ms);
  
 private:
  void Clear(DictTable& dict);
  bool ExpandIfNeed();
  DictEntry* InsertRaw(TKey key);
  DictEntry* ReplaceRaw(TKey key);
  inline bool IsRehashing() { return rehashidx_ != -1 }
  void RehashStep();
  int RehashNStep(int n);
  int KeyIndex(TKey key);
  DictEntry* Find(TKey key);
};

void EnableResize() {
  can_resize = true;
}
void DisableResize() {
  can_resize = false;
}

void SetHashSeed(uint32_t seed) {
  hash_seed = seed;
}
uint32_t GetHashSeed() {
  return hash_seed;
}

uint32_t MurmurHash2(const void* key, int len);
uint32_t GenCaseHashFunction(const unsigned char *buf, int len)


#endif
