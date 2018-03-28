# Sds
## Introduction
Sds(simple dynamic string)是redis作者定义的一个字符串类型。该类型比较简单，这里直接列举该类型和c字符串的主要区别：

| C 字符串                                         | SDS                                              |
| ------------------------------------------------ | ------------------------------------------------ |
| 获取字符串长度的复杂度为 O(N) 。                 | 获取字符串长度的复杂度为 O(1) 。                 |
| API 是不安全的，可能会造成缓冲区溢出。           | API 是安全的，不会造成缓冲区溢出。               |
| 修改字符串长度 N 次必然需要执行 N 次内存重分配。 | 修改字符串长度 N 次最多需要执行 N 次内存重分配。 |
| 只能保存文本数据。                               | 可以保存文本或者二进制数据。                     |
| 可以使用所有 <string.h> 库中的函数。             | 可以使用一部分 <string.h> 库中的函数。           |

## Reference
[Redis设计与实现--简单动态字符串](http://redisbook.com/preview/sds/content.html)