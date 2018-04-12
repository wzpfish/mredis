#include <vector>

#include "mredis/src/skiplist.h"
#include <gtest/gtest.h>

namespace mredis {

class SkipListTest : public ::testing::Test {
 public:
  SkipListTest() {}
 protected:
  virtual void SetUp() {
  }
  
  virtual void TearDown() {
  }
};

}