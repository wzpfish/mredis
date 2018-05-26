#include "mredis/src/intset.h"
#include <gtest/gtest.h>

#include <limits>
#include <unordered_set>
#include <unordered_map>

namespace mredis {

class IntsetTest : public ::testing::Test {
 public:
  IntsetTest(): intset_() {}
 protected:
  virtual void SetUp() {
    kvp_.insert({{0, -2}, {1, -1}, {2, 1}, {3, 2}});
    for (const auto& pair : kvp_) {
      values_.insert(pair.second);
      intset_.Add(pair.second);
    }
    ASSERT_TRUE(intset_.Length() == 4);
    
  }
  
  virtual void TearDown() {
  }

  Intset intset_;
  std::unordered_map<size_t, int64_t> kvp_;
  std::unordered_set<int64_t> values_;
};

TEST_F(IntsetTest, IntsetAdd) {
  Intset intset;
  // Add int16_t value without upgrade.
  bool success = intset.Add(1);
  ASSERT_TRUE(success);
  success = intset.Add(-1);
  ASSERT_TRUE(success);
  success = intset.Add(1);
  ASSERT_TRUE(!success);
  ASSERT_TRUE(intset.Length() == 2);

  // Add int32_t value with upgrade.
  success = intset.Add(std::numeric_limits<int32_t>::min());
  ASSERT_TRUE(success);
  success = intset.Add(std::numeric_limits<int32_t>::min());
  ASSERT_TRUE(!success);
  success = intset.Add(std::numeric_limits<int32_t>::max());
  ASSERT_TRUE(success);
  ASSERT_TRUE(intset.Length() == 4);

  // Add int64_t value with upgrade.
  success = intset.Add(std::numeric_limits<int64_t>::min());
  ASSERT_TRUE(success);
  success = intset.Add(std::numeric_limits<int64_t>::min());
  ASSERT_TRUE(!success);
  success = intset.Add(std::numeric_limits<int64_t>::max());
  ASSERT_TRUE(success);
  ASSERT_TRUE(intset.Length() == 6);
}

TEST_F(IntsetTest, IntsetRemove) {
  bool success;
  for (auto value : values_) {
    success = intset_.Remove(value);
    ASSERT_TRUE(success);
  }

  for (auto value : values_) {
    success = intset_.Remove(value);
    ASSERT_TRUE(!success);
  }
}

TEST_F(IntsetTest, IntsetFind) {
  bool success;
  for (auto value : values_) {
    success = intset_.Find(value);
    ASSERT_TRUE(success);
  }

  success = intset_.Find(-3);
  ASSERT_TRUE(!success);
}

TEST_F(IntsetTest, IntsetRandom) {
  int count = 10;
  while (--count) {
    auto value = intset_.Random();
    ASSERT_TRUE(values_.find(value) != values_.end());
  }
}

TEST_F(IntsetTest, IntsetGet) {
  for (const auto& pair : kvp_) {
    auto value = intset_.Get(pair.first);
    ASSERT_EQ(value, pair.second);
  }
}

}