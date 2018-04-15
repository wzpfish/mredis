#include <vector>
#include <cstdlib>

#include "mredis/src/skiplist.h"
#include "mredis/src/string.h"
#include <gtest/gtest.h>

namespace mredis {

TEST(SkipListTest, AddDeleteTest) {
  SkipList<String> list;

  bool success = list.Insert(String("wzp"), 1);
  ASSERT_EQ(success, true);
  
  success = list.Insert(String("xz"), 2);
  ASSERT_EQ(success, true);

  success = list.Delete(String("wzp"), 1);
  ASSERT_EQ(success, true);

  success = list.Delete(String("xz"), 2);
  ASSERT_EQ(success, true);
}

TEST(SkipListTest, RankTest) {
  SkipList<String> list;
  list.Insert(String("wzp"), 3);
  list.Insert(String("xz"), 1);
  list.Insert(String("ms"), 2);
  ASSERT_TRUE(list.GetRank(String("wzp"), 3) == 3);
  ASSERT_TRUE(list.GetRank(String("xz"), 1) == 1);
  ASSERT_TRUE(list.GetRank(String("ms"), 2) == 2);
}

TEST(SkipListTest, IteratorTest) {
  SkipList<int> list;
  for (int i = 0; i < 100; ++i) {
    int v = std::rand() % 2000;
    list.Insert(v, static_cast<double>(v + 1));
  }
  double last_score = -100;
  for (auto it = list.Begin(); it != list.End(); ++it) {
    ASSERT_TRUE(it->key + 1 == it->score);
    ASSERT_TRUE(it->score >= last_score);
    last_score = it->score;
  }
}

}