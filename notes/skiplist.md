# Skiplist

## Introduction
我们从链表开始，一步步逼近跳跃链表。

假如是常规链表，每个节点有一个指针指向下一个节点，那么搜索一个节点的复杂度是O(N)；

假如每隔2个节点多一个指针，指向后两个节点，则搜索复杂度是O(N/2)。

假如每隔4个节点再多一个指针，指向后4个节点，则搜索复杂度是O(N/4)。

那假如每隔2^i个节点都多一个指针，指向后2^i个节点呢？复杂度是O(log2N)。

上述这种结构可以保证搜索的复杂度是log级别的，但是插入和删除操作比较麻烦，因为比较难维持每隔2^i个节点多一个指针。

一个节点如果有k个指向后面节点的指针，该节点就叫level k节点。可以发现，按上述方法生成的链表，50%是level1, 25%是level2, 12.5%是level3，依此类推。

因此，我们通过对于每个节点都独自随机生成level来维持这个分布不变，我们就可以很方便的进行插入删除操作（因为不需要维持每2^i个节点的规律，而是随机。）

例如，对于有16个元素的链表，我们希望有8个level1， 4个level2。。。这样，期望复杂度也是O(log2N)。

## HyperParameters

### p
p表示level i+1节点占level i节点的比例，上面的例子是0.5

### MaxLevel
一般取log(1/p)(N)，N为list最大能存的元素个数。（为什么这么取？没仔细看。）

## Reference
* [Skip Lists: A Probabilistic Alternative to
Balanced Trees](ftp://ftp.cs.umd.edu/pub/skipLists/skiplists.pdf)