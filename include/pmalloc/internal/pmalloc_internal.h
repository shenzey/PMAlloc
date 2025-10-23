#ifndef PMALLOC_INTERNAL_H
#define PMALLOC_INTERNAL_H
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <x86intrin.h>
#include <unistd.h>
#include <libpmem.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <semaphore.h>

#define _malloc malloc
#define _free free
#define PMALLOC_BESTFIT
#define SLAB_MORPHING
#define NBANKS 6

#include "pmalloc/internal/dl.h"
#define RB_COMPACT
#include "pmalloc/internal/rb.h"

#include "pmalloc/internal/ql.h"
#include "pmalloc/internal/ph.h"

#include "pmalloc/pmalloc.h"

#include "pmalloc/internal/atomic.h"
#include "pmalloc/internal/pmalloc_internal_types.h"
#include "pmalloc/internal/prng.h"
#include "pmalloc/internal/hash.h"
#include "pmalloc/internal/mutex_pool.h"
#include "pmalloc/internal/nstime.h"
#include "pmalloc/internal/ticker.h"
#include "pmalloc/internal/smoothstep.h"

#include "pmalloc/internal/simple_list.h"
#include "pmalloc/internal/persistent.h"

#include "pmalloc/internal/minilog.h"
#ifdef PMALLOC_WBL
#include "pmalloc/internal/wbl_dtt.h"
#include "pmalloc/internal/wbl_commit.h"
#include "pmalloc/internal/wbl_log.h"
#include "pmalloc/internal/wbl_recovery.h"
#endif

/*
 * There are circular dependencies that cannot be broken without
 * substantial performance degradation.  In order to reduce the effect on
 * visual code flow, read the header files in multiple passes, with one of the
 * following cpp variables defined during each pass:
 *
 *   PMALLOC_H_TYPES   : Preprocessor-defined constants and psuedo-opaque data
 *                        types.
 *   PMALLOC_H_STRUCTS : Data structures.
 *   PMALLOC_H_EXTERNS : Extern data declarations and function prototypes.
 *   PMALLOC_H_INLINES : Inline functions.
 */
/******************************************************************************/
#define PMALLOC_H_TYPES

#include "pmalloc/internal/template.h"
#include "pmalloc/internal/sizeclass.h"
#include "pmalloc/internal/tsd.h"
#include "pmalloc/internal/extent.h"
#include "pmalloc/internal/numa_log.h"
#include "pmalloc/internal/minilog.h"
#include "pmalloc/internal/tcache.h"
#include "pmalloc/internal/bitmap.h"
#include "pmalloc/internal/slab.h"
#include "pmalloc/internal/log.h"
#include "pmalloc/internal/arena.h"

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define CACHELINE_SIZE 64
#define PAGE_SIZE 4096

#define CHUNK_BIT_SIZE_LG 12
#define CHUNK_BIT_SIZE (1ULL << CHUNK_BIT_SIZE_LG)

#define CHUNK_SIZE_LG 21
#define CHUNK_SIZE (1ULL << CHUNK_SIZE_LG)

#define LG_VADDR 48
#define LG_SIZEOF_PTR 3
#define LG_PAGE 12

#define LG_FLOOR_1(x) 0
#define LG_FLOOR_2(x) (x < (1ULL << 1) ? LG_FLOOR_1(x) : 1 + LG_FLOOR_1(x >> 1))
#define LG_FLOOR_4(x) (x < (1ULL << 2) ? LG_FLOOR_2(x) : 2 + LG_FLOOR_2(x >> 2))
#define LG_FLOOR_8(x) (x < (1ULL << 4) ? LG_FLOOR_4(x) : 4 + LG_FLOOR_4(x >> 4))
#define LG_FLOOR_16(x) (x < (1ULL << 8) ? LG_FLOOR_8(x) : 8 + LG_FLOOR_8(x >> 8))
#define LG_FLOOR_32(x) (x < (1ULL << 16) ? LG_FLOOR_16(x) : 16 + LG_FLOOR_16(x >> 16))
#define LG_FLOOR_64(x) (x < (1ULL << 32) ? LG_FLOOR_32(x) : 32 + LG_FLOOR_32(x >> 32))

#define LG_FLOOR(x) LG_FLOOR_64((x))
#define LG_CEIL(x) (LG_FLOOR(x) + (((x) & ((x)-1)) == 0 ? 0 : 1))

typedef unsigned szind_t;
typedef unsigned pszind_t;

#undef PMALLOC_H_TYPES
/******************************************************************************/
#define PMALLOC_H_STRUCTS

#include "pmalloc/internal/template.h"
#include "pmalloc/internal/sizeclass.h"
#include "pmalloc/internal/tsd.h"
#include "pmalloc/internal/extent.h"
#include "pmalloc/internal/numa_log.h"
#include "pmalloc/internal/minilog.h"
#include "pmalloc/internal/tcache.h"
#include "pmalloc/internal/bitmap.h"
#include "pmalloc/internal/slab.h"
#include "pmalloc/internal/log.h"
#include "pmalloc/internal/arena.h"

#undef PMALLOC_H_STRUCTS
/******************************************************************************/
#define PMALLOC_H_EXTERNS

#include "pmalloc/internal/template.h"
#include "pmalloc/internal/sizeclass.h"
#include "pmalloc/internal/tsd.h"
#include "pmalloc/internal/extent.h"
#include "pmalloc/internal/numa_log.h"
#include "pmalloc/internal/minilog.h"
#include "pmalloc/internal/tcache.h"
#include "pmalloc/internal/bitmap.h"
#include "pmalloc/internal/slab.h"
#include "pmalloc/internal/arena.h"
#include "pmalloc/internal/log.h"

extern uint32_t meta_size;

extern size_t opt_narenas;
extern const char PMEMPATH[2][50];

arena_t *choose_arena_hard(void);

#undef PMALLOC_H_EXTERNS
/******************************************************************************/
#define PMALLOC_H_INLINES

#include "pmalloc/internal/template.h"
#include "pmalloc/internal/sizeclass.h"
#include "pmalloc/internal/tsd.h"
#include "pmalloc/internal/extent.h"
#include "pmalloc/internal/bitmap.h"
#include "pmalloc/internal/slab.h"
#include "pmalloc/internal/numa_log.h"
#include "pmalloc/internal/minilog.h"
#include "pmalloc/internal/arena.h"
#include "pmalloc/internal/rtree.h"
#include "pmalloc/internal/log.h"

static inline arena_t *choose_arena(arena_t *arena)
{
	arena_t *ret;

	if (arena != NULL)
		return (arena);

	if (unlikely((ret = *arenas_tsd_get()) == NULL))
	{
		ret = choose_arena_hard();
		assert(ret != NULL);
	}

	return (ret);
}

#include "pmalloc/internal/tcache.h"

#undef PMALLOC_H_INLINES
/******************************************************************************/
#endif /* PMALLOC_INTERNAL_H */
