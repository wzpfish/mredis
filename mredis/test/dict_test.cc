#include "mredis/src/dict.h"
#include <gtest/gtest.h>

namespace mredis {

class DictTest : public ::testing::Test {
 public:
  DictTest(): dict_(std::hash<int>()) {}
 protected:
  virtual void SetUp() {
  }
  
  virtual void TearDown() {
  }

  Dictionary<int, int> dict_;
};

TEST_F(DictTest, InsertFetch) {
  ASSERT_EQ(dict_.Size(), static_cast<size_t>(0));
  int max_count = 1000000;
  for (int i = 0; i < max_count; ++i) {
    dict_.Insert(i, i + 1);
    int* value = dict_.Fetch(i);
    ASSERT_EQ(*value, i + 1);
  }
  ASSERT_EQ(dict_.Size(), static_cast<size_t>(max_count));

  for (int i = 0; i < max_count; ++i) {
    dict_.Erase(i);
    int* value = dict_.Fetch(i);
    ASSERT_EQ(value, nullptr);
  }
  ASSERT_EQ(dict_.Size(), static_cast<size_t>(0));
}

TEST_F(DictTest, Iterate) {
  int max_count = 1000000;
  for (int i = 0; i < max_count; ++i) {
    dict_.Insert(i, i + 1);
  }
  
  {
    int count = 0;
    for (auto it = dict_.Begin(); it != dict_.End(); ++it) {
      count++;
      ASSERT_EQ(it->key + 1, it->value);
    }
    ASSERT_EQ(count, max_count);
  }

  {
    int count = 0;
    for (auto it = dict_.SafeBegin(); it != dict_.SafeEnd(); it++) {
      count++;
      ASSERT_EQ(it->key + 1, it->value);
      dict_.Erase(it->key);
    }
    ASSERT_EQ(count, max_count);
  }
}

}