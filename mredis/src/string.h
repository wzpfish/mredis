#ifndef MREDIS_SRC_STRING_H_
#define MREDIS_SRC_STRING_H_

#include <memory>
#include <vector>
#include <cstdarg>
#include <iostream>

namespace {
  const size_t kStringMaxPreAlloc = 1024*1024;
}

namespace mredis {

class String {
private:
  unsigned int len_;
  unsigned int free_;
  char* buf_; 
  
public:
  inline size_t Len() {
    return len_;
  } 
  inline size_t Free() {
    return free_;
  }
  
  String();
  String(long long int value);
  String(const char* init);
  String(const void* init, size_t initlen);
  String(const String& s);
  String(String&& s) noexcept;
  String(std::vector<const char*> tokens, const char* sep);
  ~String();
  
  String& operator=(const String& rhs);
  String& operator=(String&& rhs) noexcept;
  bool operator <(const String& rhs) const;
  bool operator >(const String& rhs) const;
  bool operator ==(const String& rhs) const;
  friend std::ostream& operator<<(std::ostream& os, const String& string);

  void GrowZero(size_t len);

  void Cat(char c);
  void Cat(const char* t);
  void Cat(const void* t, size_t len);
  void Cat(const String& s);
  void CatFmt(const char* fmt, ...);
  void CatPrintf(const char* fmt, ...);
  void CatRepr(const char* p, size_t len);

  void Copy(const char* t);
  void Copy(const char* t, size_t len);

  void Trim(const char* cset);
  std::vector<String> Split(const char* sep, size_t seplen);

  void Range(int start, int end);
  
  void UpdateLen();

  void Clear();

  void ToLower();
  void ToUpper();
  void MapChars(const char* from, const char* to, size_t len);

  void MakeRoomFor(size_t addlen);
  void IncrLen(int incr);
  void RemoveFreeSpace();
  size_t AllocSize();

private:
  void Init(const void* t, size_t len);
  void CatPrintf(const char* fmt, std::va_list vlist);
};

std::vector<String> SplitLen(const char* s, size_t len, const char* sep, size_t seplen);

}
#endif
