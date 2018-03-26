#include <cassert>

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
  dict.table = zcalloc(realsize * sizeof(DictEntry*));
  
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

template<typename TKey, typename TValue>
bool Dictionary::Add(TKey key, TValue value) {
  DictEntry* entry = AddRaw(key);

  if (!entry) return false;
  entry->value = value;
  return true;
}

template<typename TKey, typename TValue>
DictEntry* Dictionary::AddRaw(TKey key) {
  if (IsRehashing()) RehashStep();
  
  size_t index = KeyIndex(key);
  if (index == -1) return nullptr;
  
  // Allocate and add a new DictEntry to table.
  DictTable* dict = IsRehashing() ? &dict_[1] : &dict_[0];
  DictEntry* entry = static_cast<DictEntry*>(zmalloc(sizeof(DictEntry)));
  entry->next = dict->table[index];
  dict->table[index] = entry;
  dict->used++;
  
  entry->key = key;
  return entry;
}

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
  return index;
}

template<typename TKey, typename TValue>
void Dictionary::RehashStep() {
  // Make sure no iterator is iterating the dictionary.
  if (iterator_count_ == 0) {
    RehashNStep(1);
  }
}

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
    zfree(dict_[0].table);
    dict_[0] = dict_[1];
    dict_[1].Reset();
    rehashidx_ = -1;
    return 0;
  }
  return 1;
}

}