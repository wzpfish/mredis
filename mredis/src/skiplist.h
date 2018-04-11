#ifndef MREDIS_SRC_SKIPLIST_H_
#define MREDIS_SRC_SKIPLIST_H_

#include <cstdlib>

namespace mredis {

namespace {
  const int kSkipListMaxLevel = 32;
  const float kSkipListProb = 0.5;
  
  int GetRandomLevel() {
    int level = 1;
    while ((std::rand() & 0xFFFF) < (kSkipListProb * 0xFFFF)) {
      ++level;
      if (level == kSkipListMaxLevel) break;
    }
    return level;
  }
}

struct RangeSpec {
  double min, max;
  bool minex, maxex;
};

template <typename T>
struct KeyRangeSpec{
  T* min, *max;
  bool minex, maxex;
};

template <typename T>
class SkipList {
 private:
  class SkipListNode {
   private:
    friend class SkipList;
    struct SkipListLevel {
      SkipListNode* forward;
      size_t span;
    };
    T key_;
    double score_;
    SkipListNode* backward_;
    SkipListLevel* level_;
   public:
    SkipListNode(int level) {
      backward_ = nullptr;
      level_ = new SkipListLevel[level];
      for (int i = 0; i < level; ++i) {
        level_[i].forward = nullptr;
        level_[i].span = 0;
      }
    }
    SkipListNode(int level, const T& key, double score): SkipListNode(level) {
      key_ = key;
      score_ = score;
    }
  };
  SkipListNode* head_;
  SkipListNode* tail_;
  int level_;
  size_t length_;
 public:
  SkipList();
  ~SkipList();
  size_t GetRank(T key, double score);
  bool IsInRange(const RangeSpec& spec);
  //size_t DeleteRangeByScore(const RangeSpec& range);
  //size_t DeleteRangeByKey(const KeyRangeSpec<T>& range);
  //size_t DeleteRangeByRank();
 private:
  SkipListNode* Insert(const T& key, double score);
  bool Delete(const T& key, double score);
  SkipListNode* FirstInRange(const RangeSpec& range);
  SkipListNode* LastInRange(const RangeSpec& range);
  SkipListNode* GetElementByRank(size_t rank);
  bool Compare(SkipListNode* node, const T& key, double score);
};

#pragma region Implementation
template <typename T>
SkipList<T>::SkipList() {
  head_ = new SkipListNode(kSkipListMaxLevel);
  tail_ = nullptr;
  level_ = 1;
  length_ = 0;
}

template <typename T>
SkipList<T>::~SkipList() {
  SkipListNode* node = head_;
  while (node != nullptr) {
    SkipListNode* next = node->level_[0].forward;
    delete node;
    node = next;
  }
}

/*
 * Insert a key with score into SkipList with the sorted order.
 * Return pointer of the inserted node.
 */
template <typename T>
SkipListNode* SkipList<T>::Insert(const T& key, double score) {
  SkipListNode* update[kSkipListMaxLevel];
  SkipListNode* node = head_;
  for (int i = level - 1; i >= 0; ++i) {
    while (node->level_[i].forward != nullptr 
           && Compare(node->level_[i].forward, key, score) == -1) {
      node = node->level_[i].forward;
    }
    update[i] = node;
  }
  
  // make sure the insert key and score is not the same.
  if (update[0]->level_[0].forward != nullptr 
      && Compare(update->level_[0].forward, key, score) == 0) {
    return nullptr;
  }

  int level = GetRandomLevel();
  for (int i = level_; i < level; ++i) {
    update[i] = head;
  }
  if (level > level_) level_ = level;

  SkipListNode* insert_node = new SkipListNode(level, key, score);
  for (int i = 0; i < level; ++i) {
    insert_node->level_[i].forward = update[i]->level_[i].forward;
    update[i]->level_[i].forward = insert_node;
  }
  insert_node->backward_ = (update[0] == head_) ? nullptr : update[0];
  if (insert_node->level_[0].forward != nullptr) {
    insert_node->level_[0].forward->backward_ = insert_node;
  }
  else {
    tail_ = insert_node;
  }
  return insert_node;
}

template <typename T>
void SkipList<T>::Delete(const T& key, double score) {
  
} 

/*
 * Compare SkipListNode with given key and score, return 0 if equal;
 * Return -1 if node is smaller, 1 if node is larger.
 */
template <typename T>
int SkipList<T>::Compare(SkipListNode* node, const T& key, double score) {
  if (node->score_ == score) {
    if (node->key_ == key) return 0;
    else return (node->key_ < key ? -1 : 1);
  }
  else return (node->score_ < score ? -1 : 1);
}

#pragma endregion

}
#endif