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

/* forward declaration. */
template <typename T>
class SLIterator;

template <typename T>
class SkipList {
 private:
  friend class SLIterator<T>;
  class SkipListNode {
   private:
    friend class SkipList;
    friend class SLIterator<T>;
    struct SkipListLevel {
      SkipListNode* forward;
      size_t span;
    };
    SkipListNode* backward_;
    SkipListLevel* level_;
   public:
    T key;
    double score;
    SkipListNode(int level) {
      score = std::numeric_limits<double>::min();
      backward_ = nullptr;
      level_ = new SkipListLevel[level];
      for (int i = 0; i < level; ++i) {
        level_[i].forward = nullptr;
        level_[i].span = 0;
      }
    }
    SkipListNode(int level, const T& key, double score): SkipListNode(level) {
      this->key = key;
      this->score = score;
    }
  };
  SkipListNode* head_;
  SkipListNode* tail_;
  int level_;
  size_t length_;
 public:
  using Iterator = SLIterator<T>;
  SkipList();
  SkipList(const SkipList<T>& other) = delete;
  SkipList(SkipList<T>&& other) noexcept;
  SkipList<T>& operator=(const SkipList<T>& rhs) = delete;
  SkipList<T>& operator=(SkipList<T>&& rhs) noexcept;
  ~SkipList();

  inline size_t Len() const { return length_; }
  size_t GetRank(const T& key, double score) const;
  bool Insert(const T& key, double score);
  bool Delete(const T& key, double score);

  /* Iterator related */
  inline Iterator Begin() const { return Iterator(head_->level_[0].forward); }
  inline Iterator End() const { return Iterator(nullptr); }
 private:
  SkipListNode* InternalInsert(const T& key, double score);
  int Compare(SkipListNode* node, const T& key, double score) const;
  void Release();
};

/* Iterator class to iterate the whole SkipList. */
template <typename T>
class SLIterator {
 private:
  using TSkipListNode = typename SkipList<T>::SkipListNode;
  TSkipListNode* node_;
 public:
  SLIterator(TSkipListNode* node): node_(node) {}

  SLIterator& operator++() {
    if (node_ != nullptr) {
      node_ = node_->level_[0].forward;
    }
    return *this;
  }

  SLIterator operator++(int) {
    SLIterator temp = *this;
    ++*this;
    return temp;
  }

  bool operator==(const SLIterator<T>& rhs) const {
    return node_ == rhs.node_;
  }

  bool operator!=(const SLIterator<T>& rhs) const {
    return !((*this) == rhs);
  }

  TSkipListNode& operator*() const {
    return *node_;
  }

  TSkipListNode* operator->() const {
    return node_;
  }
};

template <typename T>
SkipList<T>::SkipList() {
  head_ = new SkipListNode(kSkipListMaxLevel);
  tail_ = nullptr;
  level_ = 1;
  length_ = 0;
}

/* After move, other is not accessable. */
template <typename T>
SkipList<T>::SkipList(SkipList<T>&& other) noexcept {
  head_ = other.head_;
  tail_ = other.tail_;
  level_ = other.level_;
  length_ = other.length_;
  other.head_ = other.tail_ = nullptr;
}

template <typename T>
/* After move, rhs is not accessable. */
SkipList<T>& SkipList<T>::operator=(SkipList<T>&& rhs) noexcept {
  Release();
  head_ = rhs.head_;
  tail_ = rhs.tail_;
  level_ = rhs.level_;
  length_ = rhs.length_;
  rhs.head_ = rhs.tail_ = nullptr;
  return *this;
}

template <typename T>
SkipList<T>::~SkipList() {
  Release();
}

/* Get the 1-based rank for a given key. */
template <typename T>
size_t SkipList<T>::GetRank(const T& key, double score) const {
  SkipListNode* node = head_;
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

/* Insert a key to skiplist.
 * Return true if success, false if key already exists. */
template <typename T>
bool SkipList<T>::Insert(const T& key, double score) {
  SkipListNode* node = InternalInsert(key, score);
  return (node != nullptr);
}

/* Delete a key from skiplist. 
 * Return true if success, false if key not exists. */ 
template <typename T>
bool SkipList<T>::Delete(const T& key, double score) {
  SkipListNode* update[kSkipListMaxLevel];

  SkipListNode* node = head_;
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
    return false;
  }

  // update every influenced node.
  node = update[0]->level_[0].forward;
  for (int i = 0; i < level_; ++i) {
    if (update[i]->level_[i].forward == node) {
      update[i]->level_[i].forward = node->level_[i].forward;
      update[i]->level_[i].span += (node->level_[i].span - 1);
    }
    else {
      update[i]->level_[i].span--;
    }
  }

  
  // update tail of SkipList
  if (node->level_[0].forward != nullptr) {
    node->level_[0].forward->backward_ = node->backward_;
  }
  else {
    tail_ = node->backward_;
  }
  
  // update max level of SkipList.
  while (level_ > 1 && head_->level_[level_ - 1].forward == nullptr) {
    level_--;
  }
  length_--;

  delete node;
  return true;
}

/* Insert a key with score into SkipList with the sorted order.
 * Return pointer of the inserted node. */
template <typename T>
typename SkipList<T>::SkipListNode* SkipList<T>::InternalInsert(const T& key, double score) {
  SkipListNode* update[kSkipListMaxLevel];
  size_t rank[kSkipListMaxLevel];

  SkipListNode* node = head_;
  for (int i = level_ - 1; i >= 0; --i) {
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
      && Compare(update[0]->level_[0].forward, key, score) == 0) {
    return nullptr;
  }

  int level = GetRandomLevel();
  for (int i = level_; i < level; ++i) {
    update[i] = head_;
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

/* Compare SkipListNode with given key and score, return 0 if equal;
 * Return -1 if node is smaller, 1 if node is larger. */
template <typename T>
int SkipList<T>::Compare(SkipListNode* node, const T& key, double score) const {
  if (node->score == score) {
    if (node->key == key) return 0;
    else return (node->key < key ? -1 : 1);
  }
  else return (node->score < score ? -1 : 1);
}

/* Release the nodes in Skiplist. */
template <typename T>
void SkipList<T>::Release() {
  SkipListNode* node = head_;
  while (node != nullptr) {
    SkipListNode* next = node->level_[0].forward;
    delete node;
    node = next;
  }
}

}
#endif