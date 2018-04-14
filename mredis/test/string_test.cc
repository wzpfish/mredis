#include "mredis/src/string.h"
#include <gtest/gtest.h>

namespace mredis {

TEST(StringTest, ConstructorTest) {
  auto s1 = String();
  ASSERT_EQ(s1.Len(), static_cast<size_t>(0));
  
  auto s2 = String("hello, wzpfish.");
  auto s3 = String("hello, wzpfish.!!!!", 15);
  ASSERT_TRUE(s2 == s3);

  auto s4 = String(s2);
  auto s5 = String(std::vector<const char*>{"hello", "wzpfish."}, ", ");
  ASSERT_TRUE(s2 == s3);
  ASSERT_TRUE(s2 == s4);
  ASSERT_TRUE(s2 == s5);

  auto s6 = String(123456789);
  auto s7 = String("123456789");
  ASSERT_TRUE(s6 == s7);
}


TEST(StringTest, CatTest) {
  auto s1 = String("hello");
  s1.Cat(',');
  ASSERT_TRUE(s1 == String("hello,"));

  s1.Cat(" wzpfish");
  ASSERT_TRUE(s1 == String("hello, wzpfish"));
  
  s1.Cat("!!!!", 1);
  ASSERT_TRUE(s1 == String("hello, wzpfish!"));

  auto s2 = String(" hello");
  s1.Cat(s2);
  ASSERT_TRUE(s1 == String("hello, wzpfish! hello"));

  auto s3 = String("B");
  s1.CatFmt(", %i%S", 2, &s3);
  ASSERT_TRUE(s1 == String("hello, wzpfish! hello, 2B"));

  s1.CatPrintf("%d%c!", 2, 'B');
  ASSERT_TRUE(s1 == String("hello, wzpfish! hello, 2B2B!"));
  
  s1 = String();
  s1.CatRepr("hello\tworld\r\n", 13);
  ASSERT_TRUE(s1 == String("\"hello\\tworld\\r\\n\""));
}

}