#ifndef MREDIS_SRC_DICT_H_
#define MREDIS_SRC_DICT_H_

#include <cassert>
#include <chrono>
#include <ratio>
#include <cstdlib>
#include <cctype>

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

  size_t NextPower(size_t size) {
    if (size >= kLongMax) return kLongMax;

    size_t power = kTableInitSize;
    while (power < size) {
      power *= 2;
    }
    return power;
  }
}

namespace mredis {

template<typename TKey, typename TValue>
class DIterator;

/* TKey must support the hash function which passed in.
 * TValue must have default constructor and copy assignment operator.
 */
template<typename TKey, typename TValue>
class Dictionary {
 private:
  friend class DIterator<TKey, TValue>;
  struct DictEntry {
   public:
    TKey key;
    TValue value;
   private:
    friend class Dictionary<TKey, TValue>;
    friend class DIterator<TKey, TValue>;
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

  // 局部作用域变量不会初始化，需要手动reset!
  DictTable dict_[2];
  long rehashidx_;
  int iterator_count_;

  std::function<size_t(const TKey& key)> hash_func_;

 public:
  using Iterator = DIterator<TKey, TValue>;
  Dictionary(std::function<size_t(const TKey&)> hash_func) 
      : rehashidx_(-1), iterator_count_(0), hash_func_(hash_func) {
    dict_[0].Reset();
    dict_[1].Reset();
  }
  ~Dictionary();
  inline size_t Size() { return dict_[0].used + dict_[1].used; }
  inline size_t Capacity() { return IsRehashing() ? dict_[1].size : dict_[0].size; }
  bool Expand(size_t size);
  bool Shrink();
  bool Insert(const TKey& key, TValue value);
  bool Replace(const TKey& key, TValue value);
  bool Erase(const TKey& key);
  TValue* Fetch(const TKey& key);
  TKey* FetchRandom();
  std::vector<std::pair<TKey*, TValue*>> FetchSome(int count);
  void Clear();
  size_t RehashMilliseconds(int ms);
  Iterator SafeBegin();
  Iterator SafeEnd();
  Iterator Begin();
  Iterator End();
  size_t FingerPrint();
  
 private:
  void Clear(DictTable& dict);
  bool ExpandIfNeed();
  DictEntry* InsertRaw(const TKey& key);
  DictEntry* ReplaceRaw(const TKey& key);
  inline bool IsRehashing() { return rehashidx_ != -1; }
  void RehashStep();
  int RehashNStep(int n);
  int KeyIndex(const TKey& key);
  DictEntry* Find(const TKey& key);
};

template <typename TKey, typename TValue>
class DIterator {
 private:
  using TDictionary = Dictionary<TKey, TValue>;
  TDictionary* dictionary_;
  int dict_index_;
  int64_t index_;
  size_t fingerprint_;
  bool safe_;
  typename TDictionary::DictEntry* entry_;
  typename TDictionary::DictEntry* next_entry_;
 public:
  DIterator(TDictionary* dictionary, bool safe) {
    dictionary_ = dictionary;
    dict_index_ = 0;
    index_ = -1;
    safe_ = safe;
    entry_ = next_entry_ = nullptr;
  }

  ~DIterator() {
    if (!(dict_index_ == 0 && index_ == -1)) {
      if (safe_) {
        dictionary_->iterator_count_--;
      }
      else {
        assert(dictionary_->FingerPrint() == fingerprint_);
      }
    }
  }

  DIterator& operator++() {
    while(true) {
      if (entry_ == nullptr) {
        typename TDictionary::DictTable* dict = &dictionary_->dict_[dict_index_];
        // If it's the initial iterator.
        if (dict_index_ == 0 && index_ == -1) {
          if (safe_) dictionary_->iterator_count_++;
          else fingerprint_ = dictionary_->FingerPrint();
        }
        index_++;
        if (static_cast<size_t>(index_) >= dict->size) {
          if (dictionary_->IsRehashing() && dict_index_ == 0) {
            dict_index_ = 1;
            index_ = 0;
            dict = &dictionary_->dict_[dict_index_];
          }
          else break;
        }
        entry_ = dict->table[index_];
      }
      else {
        entry_ = next_entry_;
      }
      if (entry_ != nullptr) {
        next_entry_ = entry_->next;
        return *this;
      }
    }
    entry_ = nullptr;
    return *this;
  }

  DIterator operator++(int) { 
    DIterator temp = *this;
    ++*this;
    return temp;
  }

  bool operator==(const DIterator<TKey, TValue>& rhs) const {
    return dictionary_ == rhs.dictionary_ && entry_ == rhs.entry_;
  }

  bool operator!=(const DIterator<TKey, TValue>& rhs) const {
    return !((*this) == rhs);
  }

  DIterator& operator*() {
    return *entry_;
  }
  
  typename TDictionary::DictEntry* operator->() {
    return entry_;
  }
  
};

template<typename TKey, typename TValue>
Dictionary<TKey, TValue>::~Dictionary() {
  Clear(dict_[0]);
  Clear(dict_[1]);
}

template<typename TKey, typename TValue>
void Dictionary<TKey, TValue>::Clear(DictTable& dict)  {
  for (size_t i = 0; i < dict.size && dict.used > 0; ++i) {
    DictEntry* entry = dict.table[i];
    while (entry) {
      DictEntry* next = entry->next;
      delete entry;
      dict.used--;
      entry = next;
    }
  }
  delete[] dict.table;
  dict.Reset();
}

// Expand the hash table to given size.
// Return true if expand success, false if nothing happen.
template<typename TKey, typename TValue>
bool Dictionary<TKey, TValue>::Expand(size_t size) {
  if (IsRehashing() || dict_[0].used > size) return false;

  size_t realsize = NextPower(size);
  if (realsize == dict_[0].size) return false;
  
  DictTable dict;
  dict.size = realsize;
  dict.sizemask = realsize - 1;
  dict.used = 0;
  // Must add (), else the DictEntry* is not set to nullptr.
  // See: https://www.cnblogs.com/haoyijing/p/5815035.html
  dict.table = new DictEntry*[realsize]();
  
  // No need to rehash if dict_[0] is empty.
  if (dict_[0].table == nullptr) {
    dict_[0] = dict;
    return true;
  }

  // Prepare for rehashing.
  dict_[1] = dict;
  rehashidx_ = 0;
  return true;
}

template<typename TKey, typename TValue>
bool Dictionary<TKey, TValue>::ExpandIfNeed() {
  if (IsRehashing()) return true;
  
  // If table is empty, expand to init size.
  if (dict_[0].size == 0) return Expand(kTableInitSize);
  
  // Expand to 2*used size.
  size_t ratio = dict_[0].used / dict_[0].size;
  if (ratio >= kCanResizeRatio && 
      (can_resize || ratio >= kForceResizeRatio)) {
    return Expand(dict_[0].used * 2);
  }
  return true;
}

/*
 * Return true if Shrink success, false if nothing happened.
 */
template<typename TKey, typename TValue>
bool Dictionary<TKey, TValue>::Shrink() {
  if (IsRehashing() || can_resize) return false;

  size_t size = dict_[0].used;
  if (size < kTableInitSize) size = kTableInitSize;
  return Expand(size);
}

/* Return true if successfully add key, value to dictionary.
 * Return false if key already exists.
 */
template<typename TKey, typename TValue>
bool Dictionary<TKey, TValue>::Insert(const TKey& key, TValue value) {
  DictEntry* entry = InsertRaw(key);

  if (!entry) return false;
  entry->value = value;
  return true;
}

/* Return a DictEntry of given key.
 * Return nullptr if key already exists.
 */
template<typename TKey, typename TValue>
typename Dictionary<TKey, TValue>::DictEntry* Dictionary<TKey, TValue>::InsertRaw(const TKey& key) {
  // Do one increment rehash step.
  if (IsRehashing()) RehashStep();
  
  int index = KeyIndex(key);
  if (index == -1) return nullptr;
  
  // Allocate and add a new DictEntry to table.
  DictTable* dict = IsRehashing() ? &dict_[1] : &dict_[0];
  DictEntry* entry = new DictEntry;
  entry->next = dict->table[index];
  dict->table[index] = entry;
  dict->used++;
  
  entry->key = key;
  return entry;
}

/* Return the bucket index of the key.
 * Return -1 if key already exists.
 */
template<typename TKey, typename TValue>
int Dictionary<TKey, TValue>::KeyIndex(const TKey& key) {
  ExpandIfNeed();
  
  size_t hash = hash_func_(key);
  size_t index;
  for (int i = 0; i <= 1; ++i) {
    index = hash & dict_[i].sizemask;
    DictEntry* entry = dict_[i].table[index];
    while (entry != nullptr) {
      if (entry->key == key) return -1;
      entry = entry->next;
    }
    if (!IsRehashing()) break;
  }
  return static_cast<int>(index);
}

template<typename TKey, typename TValue>
void Dictionary<TKey, TValue>::RehashStep() {
  // Make sure no iterator is iterating the dictionary.
  if (iterator_count_ == 0) {
    RehashNStep(1);
  }
}

// Return 1 if rehash is still in progress.
// Return 0 if rehash is done.
template<typename TKey, typename TValue>
int Dictionary<TKey, TValue>::RehashNStep(int n) {
  if (!IsRehashing()) return 0;
  
  // RehashNStep stop after visist empty_vists empty buckets.
  int empty_vists = n * 10;
  while (n-- && dict_[0].used != 0) {
    assert(dict_[0].size > static_cast<unsigned long>(rehashidx_));
    while (dict_[0].table[rehashidx_] == nullptr) {
      rehashidx_++;
      if (--empty_vists == 0) return 1;
    }
    
    // Rehash one bucket.
    DictEntry* entry = dict_[0].table[rehashidx_];
    while (entry) {
      DictEntry* next = entry->next;
      size_t index = hash_func_(entry->key) & dict_[1].sizemask;
      entry->next = dict_[1].table[index];
      dict_[1].table[index] = entry;
      dict_[0].used--;
      dict_[1].used++;
      entry = next; 
    }
    dict_[0].table[rehashidx_++] = nullptr;
  }
  
  // Check if we rehashed all buckets.
  if (dict_[0].used == 0) {
    delete[] dict_[0].table;
    dict_[0] = dict_[1];
    dict_[1].Reset();
    rehashidx_ = -1;
    return 0;
  }
  return 1;
}

/* Return true if we add a new entry.
 * Return false if we replace the value of an old entry.
 */
template<typename TKey, typename TValue>
bool Dictionary<TKey, TValue>::Replace(const TKey& key, TValue value) {
  if (Add(key, value)) return true;
  
  DictEntry* entry = Find(key);
  assert(entry != nullptr);

  // need to free the old value?
  entry->value = value;
  return false;
}

template<typename TKey, typename TValue>
typename Dictionary<TKey, TValue>::DictEntry* Dictionary<TKey, TValue>::ReplaceRaw(const TKey& key) {
  DictEntry* entry = Find(key);
  return (entry == nullptr) ? AddRaw(key) : entry;
}

/* Return entry of the key if key found.
 * Return nullptr if key is not found.
 */
template<typename TKey, typename TValue>
typename Dictionary<TKey, TValue>::DictEntry* Dictionary<TKey, TValue>::Find(const TKey& key) {
  if (IsRehashing()) RehashStep();
  
  if (dict_[0].size == 0) return nullptr;
  size_t hash = hash_func_(key);
  for (int i = 0; i <= 1; ++i) {
    size_t index = hash & dict_[i].sizemask;
    DictEntry* entry = dict_[i].table[index];
    while (entry) {
      if (entry->key == key) return entry;
      entry = entry->next;
    }
    if (!IsRehashing()) break;
  }
  return nullptr;
}

template<typename TKey, typename TValue>
bool Dictionary<TKey, TValue>::Erase(const TKey& key) {
  if (dict_[0].size == 0) return false;

  if (IsRehashing()) RehashStep();
  
  size_t hash = hash_func_(key);
  for (int i = 0; i <= 1; ++i) {
    size_t index = hash & dict_[i].sizemask;
    DictEntry* entry = dict_[i].table[index];
    DictEntry* prev = nullptr;
    while (entry) {
      if (entry->key == key) {
        if (prev == nullptr) {
          dict_[i].table[index] = entry->next;
        }
        else {
          prev->next = entry->next;
        }
        delete entry;
        dict_[i].used--;
        return true;
      }
      prev = entry;
      entry = entry->next;
    }
    if (!IsRehashing()) break;
  }
  return false;
}

template<typename TKey, typename TValue>
TValue* Dictionary<TKey, TValue>::Fetch(const TKey& key) {
  DictEntry* entry = Find(key);
  if (entry != nullptr) {
    return &entry->value;
  }
  return nullptr;
}

template<typename TKey, typename TValue>
void Dictionary<TKey, TValue>::Clear() {
  Clear(dict_[0]);
  Clear(dict_[1]);
  rehashidx_ = -1;
  iterator_count_ = 0;
}

template<typename TKey, typename TValue>
size_t Dictionary<TKey, TValue>::RehashMilliseconds(int ms) {
  auto start = std::chrono::high_resolution_clock::now();
  size_t rehash_step = 0;
  // why rehash 100 steps?
  while (RehashNStep(100)) {
    rehash_step += 100;
    auto current = std::chrono::high_resolution_clock::now();
    std::chrono::duration<int, std::milli> diff = current - start;
    if (diff.count() > ms) break;
  }
  return rehash_step;
}

// Fetch a random key from dictionary.
// Return null if no key found.
template<typename TKey, typename TValue>
TKey* Dictionary<TKey, TValue>::FetchRandom() {
  if (dict_[0].used + dict_[1].used == 0) return nullptr;
  
  if (IsRehashing()) RehashStep();

  DictEntry* entry;
  if (IsRehashing()) {
    // Repeat to find a non-empty bucket.
    while (true) {
      int index = rehashidx_ + std::rand() % (dict_[0].size + dict_[1].size - rehashidx_);
      if (index >= dict_[0].size) {
        entry = dict_[0].table[index];
      }
      else {
        entry = dict_[1].table[index - dict_[0].size];
      }
      if (entry != nullptr) break;
    }
  }
  else {
    while (true) {
      int index = std::rand() & dict_[0].sizemask;
      entry = dict_[0].table[index];
      if (entry != nullptr) break;
    }
  }
  
  int len = 0;
  DictEntry* p = entry;
  while (p) {
    p = p->next;
    len++;
  }
  int index = std::rand() % len;
  while(index--) entry = entry->next;
  return &entry->key;
}

// Sample some continuous kv pairs from dictionary at a random location,
// Pairs maybe empty or have less count than given param.
template<typename TKey, typename TValue>
std::vector<std::pair<TKey*, TValue*>> Dictionary<TKey, TValue>::FetchSome(int count) {
  std::vector<std::pair<TKey*, TValue*>> result;
  unsigned long used = dict_[0].used + dict_[1].used;
  if (used == 0) return result;
  if (used < count) count = used;
  
  for (int i = 0; i < count; ++i) {
    if (IsRehashing()) RehashStep();
    else break;
  }
  
  int num_dict = IsRehashing() ? 2 : 1;
  unsigned long maxsizemask = dict_[0].sizemask;
  if (IsRehashing() && dict_[1].sizemask > maxsizemask) {
    maxsizemask = dict_[1].sizemask;
  }
  int rand_index = std::rand() & maxsizemask;
  int empty_len = 0;
  int max_steps = count * 10;
  while (result.size() && max_steps--) {
    for (int i = 0; i < num_dict; ++i) {
      if (num_dict == 2 && i == 0 && rand_index < rehashidx_) {
        if (rand_index > dict_[1].size) {
          rand_index = rehashidx_;
        }
        continue;
      }
      if (rand_index >= dict_[i].size) continue;

      DictEntry* entry = dict_[i].table[rand_index];
      if (entry == nullptr) {
        empty_len++;
        // why need these conditions?
        if (empty_len >= 5 && empty_len > count) {
          rand_index = std::rand() & maxsizemask;
          empty_len = 0;
        }
      }
      else {
        empty_len = 0;
        while (entry) {
          result.push_back(std::make_pair(&entry->key, &entry->value));
          if (result.size() == count) return result;
        }
      }
    }
    rand_index = (rand_index + 1) & maxsizemask;
  }
  return result;
}

template<typename TKey, typename TValue>
typename Dictionary<TKey, TValue>::Iterator Dictionary<TKey, TValue>::SafeBegin() {
  Iterator it = Iterator(this, true);
  it++;
  return it;
}

template<typename TKey, typename TValue>
typename Dictionary<TKey, TValue>::Iterator Dictionary<TKey, TValue>::SafeEnd() {
  Iterator it = Iterator(this, true);
  return it;
}

template<typename TKey, typename TValue>
typename Dictionary<TKey, TValue>::Iterator Dictionary<TKey, TValue>::Begin() {
  Iterator it = Iterator(this, false);
  it++;
  return it;
}

template<typename TKey, typename TValue>
typename Dictionary<TKey, TValue>::Iterator Dictionary<TKey, TValue>::End() {
  Iterator it = Iterator(this, false);
  return it;
}

/* Hash for a dictionary, mainly use pointer, size and used for
 * two internal dicts.
 */
template <typename TKey, typename TValue>
size_t Dictionary<TKey, TValue>::FingerPrint() {
  size_t integers[6] = {
    reinterpret_cast<size_t>(dict_[0].table),
    dict_[0].size,
    dict_[0].used,
    reinterpret_cast<size_t>(dict_[1].table),
    dict_[1].size,
    dict_[1].used
  };

  size_t hash = 0;
  for (int i = 0; i < 6; ++i) {
    hash += integers[i];
    /* For the hashing step we use Tomas Wang's 64 bit integer hash. */
    hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
    hash = hash ^ (hash >> 24);
    hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
    hash = hash ^ (hash >> 14);
    hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
    hash = hash ^ (hash >> 28);
    hash = hash + (hash << 31);
  }
  return hash;
}

/* MurmurHash2, by Austin Appleby
// Note - This code makes a few assumptions about how your machine behaves -
// 1. We can read a 4-byte value from any address without crashing
// 2. sizeof(int) == 4
//
// And it has a few limitations -
//
// 1. It will not work incrementally.
// 2. It will not produce the same results on little-endian and big-endian
//    machines.    */
uint32_t MurmurHash2(const void * key, int len){
  /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
  uint32_t seed = hash_seed;
  const uint32_t m = 0x5bd1e995;
  const int r = 24;

  /* Initialize the hash to a 'random' value */
  uint32_t h = seed ^ len;

  /* Mix 4 bytes at a time into the hash */
  const unsigned char * data = (const unsigned char *)key;

  while (len >= 4) {
    uint32_t k = *(uint32_t*)data;

    k *= m;
    k ^= k >> r;
    k *= m;

    h *= m;
    h ^= k;

    data += 4;
    len -= 4;
  }

  /* Handle the last few bytes of the input array  */

  switch(len)
  {
  case 3: h ^= data[2] << 16;
  case 2: h ^= data[1] << 8;
  case 1: h ^= data[0];
      h *= m;
  };

  /* Do a few final mixes of the hash to ensure the last few
  // bytes are well-incorporated.  */
  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;

  return h;
}

/* And a case insensitive hash function (based on djb hash) */
uint32_t GenCaseHashFunction(const unsigned char *buf, int len) {
    uint32_t hash = hash_seed;
    while (len--)
        hash = ((hash << 5) + hash) + (std::tolower(*buf++)); /* hash * 33 + c */
    return hash;
}

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

}

#endif
