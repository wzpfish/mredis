# zmalloc
## Introduction
该文件中的方法主要用于实时准确地记录实际分配的内存大小，即每次申请内存和释放内存的时候都要对使用的内存大小和释放的内存大小作记录。

该文件主要包括如下函数：

|函数名|解释|
|---|---|
|`void *zmalloc(size_t size)`|`zmalloc`与`malloc`不同的是在申请内存时会多申请一块大小为`void *sizeof(size_t)`的内存，用于记录size（即用户想要申请的内存大小）。同时，将全局`used_memory`加上实际申请到的内存大小。注意，实际申请到的内存大小=sizeof(size_t) + size + 位偏移。这个位偏移是因为`std::malloc`分配的内存是对齐的，即是`sizeof(size_t)`的倍数，而不是申请多少就分配多少。|
|`void *zcalloc(size_t size)`|同上，带内存记录的`calloc`|
|`void *zrealloc(void *ptr, size_t size)`|同上，带内存记录的`realloc`|
|`void zfree(void *ptr)`|带内存记录的`free`|
|`char *zstrdup(const char *s)`|duplicate一个c字符串，并记录内存使用|
|`size_t zmalloc_used_memory()`|返回`used_momory`，即实际分配的内存大小|
|`size_t zmalloc_get_rss()`|返回resident set size，即进程实际占用的物理内存大小（不包括被swap到磁盘中的page）。具体计算方法为，`/proc/[pid]/stat`获得进程所占的内存分页个数，`sysconf`获得每个内存分页的大小，两者相乘即为rss大小。|
|`float zmalloc_get_fragmentation_ratio(size_t rss)`|我的理解为物理内存中的大小占分配总内存大小的比例。计算公式为：`rss/used_memory`|
|`size_t zmalloc_get_smap_bytes_by_field(char *field)`|获取`/proc/self/smaps`中指定字段的数值(page count)，如: * Rss: 1840 kB, Private_Clean: 1840 kB, Private_Dirty: 0 kB|
|`size_t zmalloc_get_private_dirty()`|获取smaps中private_dirty的值，即修改过的私有内存分页个数|

## Reference
- [PROC(5) Linux Programmer's Manual](http://man7.org/linux/man-pages/man5/proc.5.html)
- 现代操作系统第三章：存储管理