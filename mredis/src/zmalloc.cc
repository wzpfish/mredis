#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#include <atomic>

#include "mredis/src/zmalloc.h"

namespace {
  int zmalloc_thread_safe_enabled = 0;
  std::atomic<size_t> used_memory(0);

  // From /proc/[pid]/stat manual:
  // Resident Set Size: number of pages the process has
  // in real memory. This is just the pages which count
  // toward text, data, or stack space. This does not
  // include pages which have not been demand-loaded in,
  // or which are swapped out.
  size_t proc_stat_rss() {
    size_t rss;
    char buf[4096];
    char filename[256];
    int fd, count;
    char *p, *x;

    snprintf(filename,256,"/proc/%d/stat",getpid());
    if ((fd = open(filename,O_RDONLY)) == -1) return 0;
    if (read(fd,buf,4096) <= 0) {
        close(fd);
        return 0;
    }
    close(fd);

    p = buf;
    count = 23; /* RSS is the 24th field in /proc/<pid>/stat */
    while(p && count--) {
        p = std::strchr(p,' ');
        if (p) p++;
    }
    if (!p) return 0;
    x = std::strchr(p,' ');
    if (!x) return 0;
    *x = '\0';
    rss = std::strtoll(p,NULL,10);
    return rss;
  }

  // log for out of memory error.
  void zmalloc_oom_handler(size_t size) {
    fprintf(stderr, "zmalloc: Out of memory trying to allocate %zu bytes\n", size);
    fflush(stderr);
    abort();
  }
}

namespace mredis {
  #define PREFIX_SIZE (sizeof(size_t))
  
  #define update_zmalloc_stat_alloc(__n) do { \
    size_t _n = (__n); \
    if (_n & (sizeof(size_t) - 1)) { \
      _n += sizeof(size_t) - (_n & (sizeof(size_t) - 1)); \
    } \
    if (zmalloc_thread_safe_enabled) { \
      used_memory.fetch_add(_n); \
    } \
    else { \
      used_memory += _n; \
    } \
  } while(0)

  #define update_zmalloc_stat_free(__n) do { \
    size_t _n = (__n); \
    if (_n & (sizeof(size_t) - 1)) { \
      _n += sizeof(size_t) - (_n & (sizeof(size_t) - 1)); \
    } \
    if (zmalloc_thread_safe_enabled) { \
      used_memory.fetch_sub(_n); \
    } \
    else { \
      used_memory -= _n; \
    } \
  } while(0)
  
  // malloc a memory, restore the required memory size
  // and count the actually allocated memory size.
  void *zmalloc(size_t size) {
    void *p = std::malloc(size + PREFIX_SIZE);
    if (p == nullptr) zmalloc_oom_handler(size);

    *((size_t*)p) = size;
    update_zmalloc_stat_alloc(size + PREFIX_SIZE);
    return (char*)p + PREFIX_SIZE;
  }
  
  // The only difference between zmalloc is zcalloc initialize
  // the allocated memory with zero value.
  void *zcalloc(size_t size) {
    void *p = std::calloc(1, size + PREFIX_SIZE);
    if (p == nullptr) zmalloc_oom_handler(size);

    *((size_t*)p) = size;
    update_zmalloc_stat_alloc(size + PREFIX_SIZE);
    return (char*)p + PREFIX_SIZE;
  }
  
  void *zrealloc(void *ptr, size_t size) {
    if (ptr == nullptr) return zmalloc(size);
    
    void *realptr = (char*)ptr - PREFIX_SIZE;
    size_t oldsize = *((size_t*)realptr);
    void *newptr = realloc(realptr, size + PREFIX_SIZE);
    if (newptr == nullptr) zmalloc_oom_handler(size);

    *((size_t*)newptr) = size;
    update_zmalloc_stat_free(oldsize);
    update_zmalloc_stat_alloc(size);
    return (char*)newptr + PREFIX_SIZE; 
  }

  void zfree(void *ptr) {
    void *realptr = (char*)ptr - PREFIX_SIZE;
    size_t size = *((size_t*)realptr);
    free(realptr);
    update_zmalloc_stat_free(size + PREFIX_SIZE);
  }
  
  char *zstrdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *ptr = (char*)zmalloc(len);
    memcpy(ptr, s, len);
    return ptr;
  }
  
  size_t zmalloc_used_memory() {
    return used_memory.fetch_add(0);
  }
  
  void zmalloc_enable_thread_safeness() {
    zmalloc_thread_safe_enabled = 1;
  }
  
  size_t zmalloc_get_rss() {
    int page_size = sysconf(_SC_PAGESIZE);
    size_t rss_page_count = proc_stat_rss();
    return (size_t)(rss_page_count * page_size);
  }

  float zmalloc_get_fragmentation_ratio(size_t rss) {
    return (float)rss / zmalloc_used_memory();
  }
  
  size_t zmalloc_get_smap_bytes_by_field(const char *field) {
    char line[1024];
    size_t bytes = 0;
    FILE *fp = fopen("/proc/self/smaps","r");
    int flen = strlen(field);

    if (!fp) return 0;
    while(fgets(line,sizeof(line),fp) != NULL) {
        if (strncmp(line,field,flen) == 0) {
            char *p = std::strchr(line,'k');
            if (p) {
                *p = '\0';
                bytes += strtol(line+flen,NULL,10) * 1024;
            }
        }
    }
    fclose(fp);
    return bytes;
  }

  size_t zmalloc_get_private_dirty() {
    return zmalloc_get_smap_bytes_by_field("Private_Dirty:");
  }

  void zlibc_free(void *ptr) {
    free(ptr);
  }
}