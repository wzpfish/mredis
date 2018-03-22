#ifndef MREDIS_INCLUDE_SDS_H_
#define MREDIS_INCLUDE_SDS_H_

#include <memory>

namespace mredis {

class Sds {
private:
  unsigned int len_;
  unsigned int free_;
  std::unique_ptr<char> buf_;  
public:
  inline size_t Len() {
    return len_;
  } 
  inline size_t Avail() {
    return free_;
  }
  Sds Newlen();  

};

}
#endif
