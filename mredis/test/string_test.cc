#include "mredis/src/string.h"
#include <gtest/gtest.h>

#include <iostream>
namespace mredis {

class StringTest: public ::testing::Test {
 public:
  
 protected:
  virtual void SetUp() {
  }
  
  virtual void TearDown() {
  }
};

TEST_F(StringTest, ConstructorTest) {
  //auto s1 = String();
  //ASSERT_EQ(s1.Len(), static_cast<size_t>(0));
  
  auto s2 = String("hello, wzpfish.");
  std::cout << s2.Len() << std::endl;
  
  //auto s3 = String("hello, wzpfish.!!!!", 15);
  //std::cout << s2.Len()  << " " << s3.Len() << std::endl;
  
  //ASSERT_TRUE(s2 == s3);
  /*
  auto s4 = String(s2);
  auto s5 = String(std::vector<const char*>{"hello", "wzpfish."}, ", ");
  ASSERT_EQ(s2, s3);
  ASSERT_EQ(s2, s4);
  ASSERT_EQ(s2, s5);

  auto s6 = String(123456789);
  auto s7 = String("123456789");
  ASSERT_EQ(s6, s7);
  */
}

}