#include "mredis/src/string.h"
#include <gtest/gtest.h>

namespace mredis {

TEST(StringTest, ConstructorTest) {
  auto s1 = String();
  ASSERT_EQ(s1.Len(), static_cast<size_t>(0));
  
  auto s2 = String("hello, wzpfish.");
  auto s3 = String("hello, wzpfish.!!!!", 15);
  ASSERT_EQ(s2, s3);

  auto s4 = String(s2);
  auto s5 = String(std::vector<const char*>{"hello", "wzpfish."}, ", ");
  ASSERT_EQ(s2, s3);
  ASSERT_EQ(s2, s4);
  ASSERT_EQ(s2, s5);

  auto s6 = String(123456789);
  auto s7 = String("123456789");
  ASSERT_EQ(s6, s7);
}

TEST(StringTest, CatTest) {
  auto s1 = String("hello");
  s1.Cat(',');
  ASSERT_EQ(s1, String("hello,"));

  s1.Cat(" wzpfish");
  ASSERT_EQ(s1, String("hello, wzpfish"));
  
  s1.Cat("!!!!", 1);
  ASSERT_EQ(s1, String("hello, wzpfish!"));

  auto s2 = String(" hello");
  s1.Cat(s2);
  ASSERT_EQ(s1, String("hello, wzpfish! hello"));

  auto s3 = String("B");
  s1.CatFmt(", %i%S", 2, &s3);
  ASSERT_EQ(s1, String("hello, wzpfish! hello, 2B"));

  s1.CatPrintf("%d%c!", 2, 'B');
  ASSERT_EQ(s1, String("hello, wzpfish! hello, 2B2B!"));
  
  s1 = String();
  s1.CatRepr("hello\tworld\r\n", 13);
  ASSERT_EQ(s1, String("\"hello\\tworld\\r\\n\""));
}

TEST(StringTest, TrimTest) {
  String s1("!hello!,~");
  s1.Trim("!,~");
  ASSERT_EQ(s1, String("hello"));

  s1 = String();
  s1.Trim("abcde");
  ASSERT_EQ(s1, String());
}

TEST(StringTest, RangeTest) {
  String s1("hello, wzpfish");
  s1.Range(0, 4);
  ASSERT_EQ(s1, String("hello"));

  s1.Range(0, -2);
  ASSERT_EQ(s1, String("hell"));

  s1.Range(-3, 2);
  ASSERT_EQ(s1, String("el"));
  
  s1 = String("hello, wzpfish");
  s1.Range(-7, -5);
  ASSERT_EQ(s1, String("wzp"));
}

TEST(StringTest, UpdateClearTest) {
  // Must specify length when init binary-safe c str.
  String s1("\0wzpfish", 8);
  ASSERT_TRUE(s1.Len() == 8);
  
  s1.UpdateLen();
  ASSERT_EQ(s1, String());

  s1 = String("\0wzpfish", 8);
  s1.Clear();
  ASSERT_EQ(s1, String());
}

TEST(StringTest, CharModifyTest) {
  String s1("hello, wzpfish");
  s1.ToUpper();
  ASSERT_EQ(s1, "HELLO, WZPFISH");
  
  s1.ToLower();
  ASSERT_EQ(s1, "hello, wzpfish");
  
  s1.MapChars("wf", "xw", 2);
  ASSERT_EQ(s1, "hello, xzpwish");
}

TEST(StringTest, SplitTest) {
  String s1("hello, wzpfish");
  auto tokens = s1.Split(", ", 2);
  ASSERT_EQ(tokens[0], String("hello"));
  ASSERT_EQ(tokens[1], String("wzpfish"));

  s1 = String("hello, wzpfish");
  tokens = s1.Split("xxx", 3);
  ASSERT_EQ(tokens[0], String("hello, wzpfish"));

  s1 = String();
  tokens = s1.Split("hello", 5);
  ASSERT_TRUE(tokens.size() == 0);
}

}