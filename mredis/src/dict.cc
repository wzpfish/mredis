#include <cassert>
#include <chrono>
#include <ratio>

#include "mredis/src/dict.h"

namespace {
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
Dictionary::~Dictionary() {
  Clear(dict_[0]);
  Clear(dict_[1]);
}

template<typename TKey, typename TValue>
void Dictionary::Clear(DictTable& dict)  {
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
template<typename TKey, typename TValue>
bool Dictionary::Expand(size_t size) {
  if (IsRehashing() || dict_[0].used > size) return false;

  size_t realsize = NextPower(size);
  if (realsize == dict_[0].size) return false;
  
  DictTable dict;
  dict.size = realsize;
  dict.sizemask = realsize - 1;
  dict.used = 0;
  dict.table = new DictEntry*[realsize];
  
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
bool Dictionary::ExpandIfNeed() {
  if (IsRehashing()) return true;
  
  // If table is empty, expand to init size.
  if (dict_[0].size == 0) return Expand(kTableInitSize);
  
  // Expand to 2*used size.
  size_t ratio = dict_[0].used / dict_[0].size;
  if (ratio >= kCanResizeRatio && 
      (canResize || ratio >= kForceResizeRatio)) {
    return Expand(dict_[0].used * 2);
  }
  return true;
}

/*
 * Return true if Shrink success, false if nothing happened.
 */
template<typename TKey, typename TValue>
bool Dictionary::Shrink() {
  if (IsRehashing() || canResize) return false;

  size_t size = dict_[0].used;
  if (size < kTableInitSize) size = kTableInitSize;
  return Expand(size);
}

/* Return true if successfully add key, value to dictionary.
 * Return false if key already exists.
 */
template<typename TKey, typename TValue>
bool Dictionary::Insert(TKey key, TValue value) {
  DictEntry* entry = InsertRaw(key);

  if (!entry) return false;
  entry->value = value;
  return true;
}

/* Return a DictEntry of given key.
 * Return nullptr if key already exists.
 */
template<typename TKey, typename TValue>
DictEntry* Dictionary::InsertRaw(TKey key) {
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
int Dictionary::KeyIndex(TKey key) {
  ExpandIfNeed();
  
  size_t hash = hash_func(key);
  size_t index;
  for (int i = 0; i <= 1; ++i) {
    index = hash & dict_[i].sizemask;
    DictEntry* entry = dict_[i].table[index];
    while (entry) {
      if (entry->key == key) return -1;
      entry = entry->next;
    }
    if (!IsRehashing()) break;
  }
  return static_cast<int>(index);
}

template<typename TKey, typename TValue>
void Dictionary::RehashStep() {
  // Make sure no iterator is iterating the dictionary.
  if (iterator_count_ == 0) {
    RehashNStep(1);
  }
}

// Return 1 if rehash is still in progress.
// Return 0 if rehash is done.
template<typename TKey, typename TValue>
int Dictionary::RehashNStep(int n) {
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
bool Dictionary::Replace(TKey key, TValue value) {
  if (Add(key, value)) return true;
  
  DictEntry* entry = Find(key);
  assert(entry != nullptr);

  // need to free the old value?
  entry->value = value;
  return false;
}

template<typename TKey, typename TValue>
DictEntry* Dicionary::ReplaceRaw(TKey key) {
  DictEntry* entry = Find(key);
  return (entry == nullptr) ? AddRaw(key) : entry;
}

/* Return entry of the key if key found.
 * Return nullptr if key is not found.
 */
template<typename TKey, typename TValue>
DictEntry* Dictionary::Find(TKey key) {
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
bool Dictionary::Erase(TKey key) {
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
bool Dictionary::TryFetch(const TKey& key, TValue* value) {
  DictEntry* entry = Find(key);
  if (entry != nullptr) {
    value = &entry->value;
    return true;
  }
  value = nullptr;
  return false;
}

template<typename TKey, typename TValue>
void Dictionary::Clear() {
  Clear(dict_[0]);
  Clear(dict_[1]);
  rehashidx_ = -1;
  iterator_count_ = 0;
}

template<typename TKey, typename TValue>
size_t Dictionary::RehashMilliseconds(int ms) {
  auto start = std::chrono::high_resolution_clock::now();
  size_t rehash_step = 0;
  // why rehash 100 steps?
  while (RehashNStep(100)) {
    rehash_step += 100;
    auto current = std::chrono::high_resolution_clock::now();
    std::chrono::duration<int, std::milli> diff = current - start;
    if (diff > ms) break;
  }
  return rehash_step;
}

}