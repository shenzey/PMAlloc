# PMAlloc: A Holistic Approach to Improving Persistent Memory Allocation

PMAlloc is a heap memory allocator for persistent memory (e.g., Intel Optane DCPMM); it emphasizes efficiently eliminating cache line reflushes and small random writes, alleviating slab-induced memory fragmentation in heap metadata management and reducing overhead induced by NUMA effect. PMAlloc has following new techniques:
* Interleaved Mapping
* Log-structured Bookkeeping
* Slab Morphing
* Local-first allocation policy / two-phase deallocation mechanism

Please read the following paper for more details: 

Zheng Dang, Shuibing He, Xuechen Zhang, Peiyi Hong, Zhenxin Li, Xinyu Chen, Haozhe Song, Xian-He Sun, and Gang Chen. "[PMAlloc: A Holistic Approach to Improving Persistent
Memory Allocation](https://dl.acm.org/doi/10.1145/3643886)", ACM Transactions on Computer Systems (TOCS), Volume 42, Issue 3-4, Article No. 7, Sept. 2024.

## Warning

The code can only be used for academic research.

We use part of jemalloc's code in our source code, please check jemalloc's [COPYRIGHT](https://github.com/jemalloc/jemalloc/blob/master/COPYING).


## Directories

* include : header files
* include/internal/pmalloc_internal.h : It includes all header files, and should be included by all source files.
* src : source files


## System Requirements

1. 2nd Generation Intel Xeon Scalable Processors
2. Intel® Optane™ Persistent Memory 100 Series
3. cmake (version > 3.10)
4. libpmem
5. libjemalloc


## Build

```
autogen.sh [d/r]
```
autogen.sh encapsulates CMakeLists.txt:
* [**d/r**] : debug/release

## Other

Default pmem directory: /mnt/pmem/pmalloc_files/,/mnt/pmem1/pmalloc_files/

To use customized mount path, modify PMEMPATH variable in src/arena.c:7.