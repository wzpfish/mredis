#ifndef MREDIS_SRC_SKIPLIST_H_
#define MREDIS_SRC_SKIPLIST_H_

#include <cstdlib>
#include <limits>

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
      score_ = std::numeric_limits<double>::min();
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
  size_t GetRank(const T& key, double score);
  //bool IsInRange(const RangeSpec& spec);
  //size_t DeleteRangeByScore(const RangeSpec& range);
  //size_t DeleteRangeByKey(const KeyRangeSpec<T>& range);
  //size_t DeleteRangeByRank();
 private:
  SkipListNode* Insert(const T& key, double score);
  bool Delete(const T& key, double score);
  //SkipListNode* FirstInRange(const RangeSpec& range);
  //SkipListNode* LastInRange(const RangeSpec& range);
  //SkipListNode* GetElementByRank(size_t rank);
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

template <typename T>
size_t SkipList<T>::GetRank(const T& key, double score) {
  SkipListNode* node = head;
  size_t rank = 0;
  for (int i = level_ - 1; i >= 0; --i) {
    while (node->level_[i].forward != nullptr
           && Compare(node->level_[i].forward, key, score) == -1) {
      rank += node->level_[i].span;
      node = node->level_[i].forward;
    }
  }
  
  node = node->level_[0].forward;
  if (node != nullptr && Compare(node, key, score) == 0) {
    return rank + 1;
  }
  return 0;
}

/*
 * Insert a key with score into SkipList with the sorted order.
 * Return pointer of the inserted node.
 */
template <typename T>
SkipListNode* SkipList<T>::Insert(const T& key, double score) {
  SkipListNode* update[kSkipListMaxLevel];
  size_t rank[kSkipListMaxLevel];

  SkipListNode* node = head_;
  for (int i = level_ - 1; i >= 0; ++i) {
    rank[i] = (i == level_ - 1) ? 0 : rank[i + 1];
    while (node->level_[i].forward != nullptr 
           && Compare(node->level_[i].forward, key, score) == -1) {
      node = node->level_[i].forward;
      rank[i] += node->level_[i].span;
    }
    update[i] = node;
  }
  
  // make sure the insert key and score not exist.
  if (update[0]->level_[0].forward != nullptr 
      && Compare(update->level_[0].forward, key, score) == 0) {
    return nullptr;
  }

  int level = GetRandomLevel();
  for (int i = level_; i < level; ++i) {
    update[i] = head;
    rank[i] = 0;
    update[i]->level_[i].span = length_;
  }
  if (level > level_) level_ = level;

  SkipListNode* insert_node = new SkipListNode(level, key, score);
  for (int i = 0; i < level; ++i) {
    insert_node->level_[i].forward = update[i]->level_[i].forward;
    update[i]->level_[i].forward = insert_node;
    insert_node->level_[i].span = update[i]->level_[i].span + rank[i] - rank[0];
    update[i]->level_[i].span = rank[0] - rank[i] + 1;
  }
  insert_node->backward_ = (update[0] == head_) ? nullptr : update[0];
  if (insert_node->level_[0].forward != nullptr) {
    insert_node->level_[0].forward->backward_ = insert_node;
  }
  else {
    tail_ = insert_node;
  }
  length_++;
  return insert_node;
}

template <typename T>
void SkipList<T>::Delete(const T& key, double score) {
  SkipListNode* update[kSkipListMaxLevel];

  SkipListNode* node = head;
  for (int i = level_ - 1; i >= 0; --i) {
    while (node->level_[i].forward != nullptr
           && Compare(node->level_[i].forward, key, score) == -1) {
      node = node->level_[i].forward;
    }
    update[i] = node;
  }
  
  // make sure next node is equal to key and score.
  if (update[0]->level_[0].forward == nullptr 
      || Compare(update[0]->level_[0].forward, key, score) != 0) {
    return;
  }

  // update every influenced node.
  node = update[0]->level_[0].forward;
  for (int i = 0; i < level_; ++i) {
    if (update[i]->level_[i].forward == node) {
      update[i]->level_[i].forward = node->forward;
      update[i]->level_[i].span += (node->level_[i].span - 1);
    }
    else {
      update[i]->level_[i].span--;
    }
  }

  
  // update tail of SkipList
  if (node->level_[0].forward != nullptr) {
    node->level_[0].forward->backward = node->backward;
  }
  else {
    tail_ = node->backward;
  }
  
  // update max level of SkipList.
  while (level_ > 1 && head_->level_[level_ - 1].forward == nullptr) {
    level_--;
  }
  length_--;

  delete node;
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