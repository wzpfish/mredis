#include <cstring>
#include <cstdarg>
#include <algorithm>
#include <string>
#include <cctype>
#include <cstdio>
#include <cassert>

#include "mredis/src/string.h"
#include "mredis/src/zmalloc.h"

namespace mredis{
  
void String::Init(const void* init, size_t initlen) {
  if (init != nullptr) {
    buf_ = static_cast<char*>(zmalloc(initlen + 1));
  }
  else {
    buf_ = static_cast<char*>(zcalloc(initlen + 1));
  }
  if (init != nullptr && initlen) {
    std::memcpy(buf_, init, initlen);
  }
  buf_[initlen] = '\0';
  len_ = initlen;
  free_ = 0;
}

String::String(): String(nullptr, 0) { }

String::String(long long int value) {
  const char* str = std::to_string(value).c_str();
  // NOTE: cannot write like this: String(str, std::strlen(str))
  Init(str, std::strlen(str));
}

String::String(const char* init) : String(init, init == nullptr ? 0 : std::strlen(init)) { }

String::String(const String& s): String(s.buf_, s.len_) { }

String::String(String&& s) noexcept {
  buf_ = s.buf_;
  len_ = s.len_;
  free_ = s.free_;
  s.buf_ = nullptr;
}

String::String(const void* init, size_t initlen) {
  Init(init, initlen);
}

String::String(std::vector<const char*> tokens, const char* sep): String() {
  for (size_t i = 0; i < tokens.size(); ++i) {
    Cat(tokens[i]);
    if (i != tokens.size() - 1) {
      Cat(sep);
    }
  }
}

String::~String() {
  if (buf_ == nullptr) return;
  zfree(buf_);
  buf_ = nullptr;
}

String& String::operator=(const String& rhs) {
  Copy(rhs.buf_, rhs.len_);
  return *this;
}

String& String::operator=(String&& rhs) noexcept {
  if (buf_ != nullptr) zfree(buf_);
  buf_ = rhs.buf_;
  len_ = rhs.len_;
  free_ = rhs.free_;
  rhs.buf_ = nullptr;
  return *this;
}

bool String::operator==(const String& rhs) const {
  if (len_ != rhs.len_) return false;
  int cmp = std::memcmp(buf_, rhs.buf_, std::min(len_, rhs.len_));
  return cmp == 0;
}

bool String::operator<(const String& rhs) const {
  int cmp = std::memcmp(buf_, rhs.buf_, std::min(len_, rhs.len_));
  if (cmp == 0) return len_ < rhs.len_;
  return cmp < 0;
}

bool String::operator>(const String& rhs) const {
  int cmp = std::memcmp(buf_, rhs.buf_, std::min(len_, rhs.len_));
  if (cmp == 0) return len_ > rhs.len_;
  return cmp > 0;
}

std::ostream& operator<<(std::ostream& os, const String& string) {
  for (size_t i = 0; i < string.len_; ++i) {
    os << string.buf_[i];
  }
  return os;
}

void String::Cat(char c) {
  MakeRoomFor(1);
  buf_[len_] = c;
  buf_[len_ + 1] = '\0';
  len_++;
  free_--;
}

void String::Cat(const char* t) {
  size_t len = t == nullptr ? 0 : std::strlen(t);
  Cat(t, len);
}

void String::Cat(const void* t, size_t len) {
  MakeRoomFor(len);

  std::memcpy(buf_ + len_, t, len);
  buf_[len_ + len] = '\0';
  len_ += len;
  free_ -= len;
}

void String::Cat(const String& s) {
  Cat(s.buf_, s.len_);
}

void String::CatFmt(const char* fmt, ...) {
  std::va_list args;
  va_start(args, fmt);
  
  const char* p = fmt;
  while (*p) {
    char next, *cstr;
    String* str;
    long long int num;
    unsigned long long int unum;
    switch (*p) {
    case '%':
      p++;
      next = *p;
      switch (*p) {
      case 's':
        cstr = va_arg(args, char*);
        Cat(cstr);
        break;
      case 'S':
        str = va_arg(args, String*);
        Cat(*str);
        break;
      case 'i':
      case 'I':
        num = (next == 'i' ? va_arg(args, int) 
                           : va_arg(args, long long));
        Cat(std::to_string(num).c_str());
        break;
      case 'u':
      case 'U':
        unum = (next == 'u' ? va_arg(args, unsigned int) 
                            : va_arg(args, unsigned long long));
        Cat(std::to_string(unum).c_str());
      default:
        Cat(next);
        break;
      }
      break;
    default:
      Cat(*p);
      break;
    }
    p++;
  }
  va_end(args);
}

void String::CatPrintf(const char* fmt, std::va_list vlist) {
  std::va_list copy;
  char staticbuf[1024], *buf = staticbuf;
  size_t buflen = std::strlen(fmt) * 2;

  if (buflen > sizeof(staticbuf)) {
    buf = static_cast<char*>(zmalloc(buflen));
  }
  else {
    buflen = sizeof(staticbuf);
  }

  while (true) {
    buf[buflen - 2] = '\0';
    va_copy(copy, vlist);
    std::vsnprintf(buf, buflen, fmt, copy);
    va_end(copy);
    // '\0' means the buf size is not enough.
    if (buf[buflen - 2] != '\0') {
      if (buf != staticbuf) zfree(buf);
      buflen *= 2;
      buf = static_cast<char*>(zmalloc(buflen));
    }
    else break;
  }

  Cat(buf);
  if (buf != staticbuf) zfree(buf);
}

void String::CatPrintf(const char* fmt, ...) {
  std::va_list args;
  va_start(args, fmt);
  CatPrintf(fmt, args);
  va_end(args);
}

void String::CatRepr(const char* p, size_t len) {
  Cat("\"", 1);
  for (size_t i = 0; i < len; ++i, ++p) {
    switch(*p) {
    case '\\':
    case '"':
      CatPrintf("\\%c", *p);
      break;
    case '\n': Cat("\\n", 2); break;
    case '\r': Cat("\\r", 2); break;
    case '\t': Cat("\\t", 2); break;
    case '\a': Cat("\\a", 2); break;
    case '\b': Cat("\\b", 2); break;
    default:
      if (std::isprint(*p)) CatPrintf("%c", *p);
      else CatPrintf("\\x%02x", (unsigned char)*p);
    }
  }
  Cat("\"", 1);
}

// Grow String to given len and fill the growed space with '\0'.
void String::GrowZero(size_t len) {
  if (len <= len_) return;
  MakeRoomFor(len - len_);
  std::memset(buf_ + len_, 0, len - len_ + 1);
  size_t totlen = len_ + free_;
  len_ = len;
  free_ = totlen - len_;
}

void String::Copy(const char* t) {
  Copy(t, std::strlen(t));
}

void String::Copy(const char* t, size_t len) {
  size_t totlen = len_ + free_;
  if (totlen < len) {
    MakeRoomFor(len - totlen);
    totlen = len_ + free_;
  }
  std::memcpy(buf_, t, len);
  buf_[len] = '\0';
  len_ = len;
  free_ = totlen - len_;
}

void String::Trim(const char* cset) {
  char* start, *end, *sp, *ep;
  start = sp = buf_;
  end = ep = buf_ + len_ - 1;
  while(sp <= end && std::strchr(cset, *sp)) sp++;
  while(ep >= start && std::strchr(cset, *ep)) ep--;
  size_t len = (sp > ep) ? 0 : (ep - sp + 1);

  if (buf_ != sp) memmove(buf_, sp, len);
  buf_[len] = '\0';
  free_ += (len_ - len);
  len_ = len;
}

std::vector<String> String::Split(const char* sep, size_t seplen) {
  return std::move(SplitLen(buf_, len_, sep, seplen));
}

void String::Range(int start, int end) {
  if (start < 0) start += len_;
  if (end < 0) end += len_;
  assert(0 <= start && start <= end && static_cast<size_t>(end) <= len_ - 1);

  size_t len = end - start + 1;
  if (start > 0) memmove(buf_, buf_ + start, len);
  buf_[len] = '\0';
  free_ += (len_ - len);
  len_ = len;
}

void String::UpdateLen() {
  int len = std::strlen(buf_);
  free_ += (len_ - len);
  len_ = len;
}

void String::Clear() {
  free_ += len_;
  len_ = 0;
  buf_[0] = '\0';
}

void String::ToLower() {
  for (size_t i = 0; i < len_; ++i) {
    buf_[i] = std::tolower(buf_[i]);
  }
}

void String::ToUpper() {
  for (size_t i = 0; i < len_; ++i) {
    buf_[i] = std::toupper(buf_[i]);
  }
}

void String::MapChars(const char* from, const char* to, size_t len) {
  for (size_t i = 0; i < len_; ++i) {
    for (size_t j = 0; j < len; ++j) {
      if (buf_[i] == from[j]) {
        buf_[i] = to[j];
        break;
      }
    }
  }
}

/* Extend a space for addlen, if free space is enough, just return.
 * Else, new a space larger than addlen.  
 */
void String::MakeRoomFor(size_t addlen) {
  if (free_ >= addlen) return;

  size_t newlen = len_ + addlen;
  if (newlen < kStringMaxPreAlloc) {
    newlen *= 2;
  }
  else {
    newlen += kStringMaxPreAlloc;
  }
  
  buf_ = static_cast<char*>(zrealloc(buf_, newlen + 1));
  free_ = newlen - len_;
}

void String::IncrLen(int incr) {
  if (incr >= 0) assert(free_ >= (unsigned int)incr);
  else assert(len_ >= (unsigned int)(-incr));
  len_ += incr;
  free_ -= incr;
  buf_[len_] = '\0';
}

void String::RemoveFreeSpace() {
  buf_ = static_cast<char*>(zrealloc(buf_, len_ + 1));
  free_ = 0;
}

size_t String::AllocSize() {
  return free_ + len_ + 1;
}

std::vector<String> SplitLen(
    const char* s, size_t len, const char* sep, size_t seplen) {
  std::vector<String> tokens;
  if (len < 1 || seplen < 1) return tokens;
  
  size_t start = 0;
  for (size_t i = 0; i < len - seplen + 1; ++i) {
    if (std::memcmp(s + i, sep, seplen) == 0) {
      tokens.push_back(String(s + start, i - start));
      start = i + seplen;
      i = start - 1;
    }
  }
  tokens.push_back(String(s + start, len - start));
  return tokens;
}

}