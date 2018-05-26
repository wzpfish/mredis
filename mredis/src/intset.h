#ifndef MREDIS_SRC_INTSET_H_
#define MREDIS_SRC_INTSET_H_

#include <cstdint>
#include <utility>
#include <cstddef>

namespace mredis {
  enum encoding: uint32_t {
    INT16 = sizeof(int16_t),
    INT32 = sizeof(int32_t),
    INT64 = sizeof(int64_t)
  };

  class Intset {
   private:
    encoding encoding_;
    size_t length_;
    int8_t* contents;
   public:
    Intset();
    bool Add(int64_t value);
    bool Remove(int64_t value);
    bool Find(int64_t value) const;
    int64_t Random() const;
    // Not use operator[] because reference may not safe.
    int64_t Get(size_t index) const;
    
    inline size_t Bytes() const { return sizeof(Intset) + length_ * encoding_; }
    inline size_t Length() const { return length_; }
    inline size_t Size() const { return sizeof(Intset) + encoding_ * length_; }
   private:
    // Search for a given value.
    // Return true and position of the value if value found.
    // Return false and position the value can be insert otherwise.
    std::pair<bool, size_t> Search(int64_t value) const;

    void Resize(size_t length);

    // Move the elements in the range of from to end, to a new position.
    // i.e. [from, length_) to [to, length_ - from + to)
    void BatchMove(size_t from, size_t to);

    int64_t Get(size_t index, encoding enc) const;

    void Set(size_t index, int64_t value);

    // Add a larger-encoding element, which will cause upgrade encoding.
    bool AddWithUpgrade(int64_t value, encoding newenc);
  };
}

#endif