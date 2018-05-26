#include "mredis/src/intset.h"
#include "mredis/src/zmalloc.h"

#include <glog/logging.h>
#include <limits>
#include <utility>
#include <tuple>
#include <cstring>
#include <random>
#include <stdexcept>

namespace mredis {

namespace {
  encoding ValueEncoding(int64_t value) {
    if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
      return encoding::INT64;
    }
    else if (value < std::numeric_limits<int16_t>::min() || value > std::numeric_limits<int16_t>::max()) {
      return encoding::INT32;
    }
    else {
      return encoding::INT16;
    }
  }
}

Intset::Intset() {
  encoding_ = encoding::INT16;
  length_ = 0;
  contents = nullptr;
}

bool Intset::Add(int64_t value) {
  // Upgrade and add the element if value encoding is larger than current.
  auto value_encoding = ValueEncoding(value);
  if (value_encoding > encoding_) {
    return AddWithUpgrade(value, value_encoding);
  }

  bool found;
  size_t pos;
  std::tie(found, pos) = Search(value);
  if (found) return false;
  // Add an value if it's not in IntSet.
  Resize(length_ + 1);
  if (pos < length_) {
    BatchMove(pos, pos + 1);
  }
  length_++;
  Set(pos, value);
  return true;
}

bool Intset::Remove(int64_t value) {
  bool found;
  size_t index;
  std::tie(found, index) = Search(value);
  if (!found) return false;
  for (size_t i = index; i < length_ - 1; ++i) {
    auto val = Get(i + 1);
    Set(i, val);
  }
  length_--;
  Resize(length_);
  return true;
}

bool Intset::Find(int64_t value) const {
  bool found;
  size_t index;
  std::tie(found, index) = Search(value);
  return found;
}

int64_t Intset::Random() const {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, length_ - 1);
  size_t index = static_cast<size_t>(dis(gen));
  return Get(index);
}

int64_t Intset::Get(size_t index) const {
  CHECK(0 <= index && index < length_) << "index not valid.";
  return Get(index, encoding_);
}

std::pair<bool, size_t> Intset::Search(int64_t value) const {
  size_t left = 0, right = length_;
  int64_t mid_value;
  while (left < right) {
    size_t mid = (left + right) >> 1;
    mid_value = Get(mid);
    if (value == mid_value) return std::make_pair(true, mid);
    else if (mid_value < value) left = mid + 1;
    else right = mid;
  }
  return std::make_pair(false, left);
}

void Intset::Resize(size_t length) {
  void* ptr = zrealloc(contents, length * encoding_);
  contents = static_cast<int8_t*>(ptr);
}

void Intset::BatchMove(size_t from, size_t to) {
  size_t count = (length_ - from) * encoding_;
  void* src, *dest;
  if (encoding_ == encoding::INT16) {
    src = reinterpret_cast<int16_t*>(contents) + from;
    dest = reinterpret_cast<int16_t*>(contents) + to;
  }
  else if (encoding_ == encoding::INT32) {
    src = reinterpret_cast<int32_t*>(contents) + from;
    dest = reinterpret_cast<int32_t*>(contents) + to;
  }
  else if (encoding_ == encoding::INT64) {
    src = reinterpret_cast<int64_t*>(contents) + from;
    dest = reinterpret_cast<int64_t*>(contents) + to;
  }
  std::memmove(dest, src, count);
}

int64_t Intset::Get(size_t index, encoding enc) const {
  CHECK(0 <= index && index < length_) << "index not valid, length_="
      << length_ << ", index=" << index;

  if (enc == encoding::INT16) {
    int16_t* ptr = reinterpret_cast<int16_t*>(contents) + index;
    return *ptr;
  }
  else if (enc == encoding::INT32) {
    int32_t* ptr = reinterpret_cast<int32_t*>(contents) + index;
    return *ptr;
  }
  else if (enc == encoding::INT64) {
    int64_t* ptr = reinterpret_cast<int64_t*>(contents) + index;
    return *ptr;
  }
  const char* message = "encoding not supported.";
  LOG(ERROR) << message;
  throw new std::invalid_argument(message);
}

void Intset::Set(size_t index, int64_t value) {
  CHECK(0 <= index && index < length_) << "index not valid, length_="
      << length_ << ", index=" << index;

  if (encoding_ == encoding::INT16) {
    int16_t* ptr = reinterpret_cast<int16_t*>(contents) + index;
    *ptr = static_cast<int16_t>(value);
  }
  else if (encoding_ == encoding::INT32) {
    int32_t* ptr = reinterpret_cast<int32_t*>(contents) + index;
    *ptr = static_cast<int32_t>(value);
  }
  else if (encoding_ == encoding::INT64) {
    int64_t* ptr = reinterpret_cast<int64_t*>(contents) + index;
    *ptr = static_cast<int64_t>(value);
  }
}

bool Intset::AddWithUpgrade(int64_t value, encoding newenc) {
  auto oldenc = encoding_;
  encoding_ = newenc;
  auto oldlen = length_;
  length_++;
  Resize(oldlen + 1);

  size_t add_to_front = (value < 0) ? 1 : 0;
  // Note: for (size_t index = oldlen - 1; index >= 0; --index)
  // this style will cause an infinite loop!
  for (size_t index = oldlen; index-- > 0; ) {
    auto val = Get(index, oldenc);
    Set(index + add_to_front, val);
  }
  
  if (add_to_front) {
    Set(0, value);
  }
  else {
    Set(oldlen, value);
  }

  return true;
}

}