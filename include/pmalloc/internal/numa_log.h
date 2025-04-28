/**
 * @file numa_log.h
 * @brief store slab modification in sequential log
 *
 * Copyright 2022. All Rights Reserved.
 *
 * Distributed under MIT license.
 * See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
 *
 */
/******************************************************************************/
#ifdef PMALLOC_H_TYPES

typedef struct numa_log_s numa_log_t;
typedef struct numa_log_entry_s numa_log_entry_t;

#define NUMA_LOG_IM

#ifdef NUMA_LOG_IM
#define NUMA_LOG_NBANKS NBANKS
#define NUMA_LOG_FILE_SIZE (NUMA_LOG_NBANKS * 512 * 4096)

#define NUMA_LOG_GROUP_SIZE (NUMA_LOG_NBANKS * CACHE_LINE_SIZE)
#define NUMA_LOG_LOG_ENTRY_NUM_PER_CACHE_LINE (CACHE_LINE_SIZE / sizeof(numa_log_entry_t))

#else

#define NUMA_LOG_FILE_SIZE (6 * 512 * 4096)

#endif

#endif /* PMALLOC_H_TYPES */
/******************************************************************************/
#ifdef PMALLOC_H_STRUCTS

struct numa_log_entry_s
{
    uint64_t info;
    uint64_t timestamp;
};

struct numa_log_s
{
#ifdef NUMA_LOG_IM
    uint32_t nle_id[2];
#else
    uint32_t nle_offset[2]; // offset of the log_entry in the *ncl* cacheline
#endif
    uint32_t limit;
    numa_log_entry_t *file_ptr[2];
    uint8_t turn;

    bitmap_t *arenas_used;
    uint nbitmaps;

    uint8_t type; // 0 for slab, 1 for entent
};

#endif /* PMALLOC_H_STRUCTS */
/******************************************************************************/
#ifdef PMALLOC_H_EXTERNS

void numa_log_init(numa_log_t *nl, arena_t *arena, uint8_t type);
void numa_log_alloc(numa_log_t *nl, uint64_t info, arena_t *target_arena);
void numa_log_flush(numa_log_t *nl);
void numa_log_slab_flusher(arena_t *arena);
void numa_log_extent_flusher(arena_t *arena);
void numa_log_recovery();

extern arena_t **arenas;
extern unsigned narenas_total;
extern sem_t **sems[2];

#endif /* PMALLOC_H_EXTERNS */
/******************************************************************************/
#ifdef PMALLOC_H_INLINES

static inline unsigned long long numa_timestamp(void)
{
    unsigned hi, lo;
    __asm__ __volatile__("rdtsc"
                         : "=a"(lo), "=d"(hi));
    return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

#endif /* PMALLOC_H_INLINES */
       /******************************************************************************/