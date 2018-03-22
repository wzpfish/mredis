#ifndef MREDIS_INCLUDE_ZMALLOC_H_
#define MREDIS_INCLUDE_ZMALLOC_H_

namespace mredis {
    void *zmalloc(size_t size);
    void *zcalloc(size_t size);
    void *zrealloc(void *ptr, size_t size);
    void zfree(void *ptr);
    char *zstrdup(const char *s);
    size_t zmalloc_used_memory(void);
    void zmalloc_enable_thread_safeness(void);
    float zmalloc_get_fragmentation_ratio(size_t rss);
    size_t zmalloc_get_rss(void);
    size_t zmalloc_get_private_dirty(void);
    size_t zmalloc_get_smap_bytes_by_field(char *field);
    void zlibc_free(void *ptr);
}

#endif