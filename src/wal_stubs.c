#include "pmalloc/internal/pmalloc_internal.h"

#ifdef PMALLOC_WBL

minilog_t *minilog[2];
uint64_t global_index[2];

minilog_t *minilog_create(unsigned numa_ind)
{
    (void)numa_ind;
    return NULL;
}

void add_minilog(minilog_t *log, uint64_t *index, uint64_t ptr)
{
    (void)log;
    (void)index;
    (void)ptr;
}

vlog_t *log_create(arena_t *arena)
{
    (void)arena;
    return NULL;
}

void *add_log(arena_t *arena, vlog_t **vlog_ptr, uint8_t type, uint64_t ptr, uint64_t size_version_id, bool slab)
{
    (void)arena;
    (void)vlog_ptr;
    (void)type;
    (void)ptr;
    (void)size_version_id;
    (void)slab;
    return NULL;
}

void add_tomb(arena_t *arena, vlog_t *vlog, shadow_chunk_t **shadow_tomb, uint64_t ptr)
{
    (void)arena;
    (void)vlog;
    (void)shadow_tomb;
    (void)ptr;
}

void flush_tomb(arena_t *arena, vlog_t **vlog_ptr, log_item_t tomb)
{
    (void)arena;
    (void)vlog_ptr;
    (void)tomb;
}

void fast_GC(vlog_t *vlog)
{
    (void)vlog;
}

void slow_GC(arena_t *arena)
{
    (void)arena;
}

void numa_log_init(numa_log_t *nl, arena_t *arena, uint8_t type)
{
    (void)nl;
    (void)arena;
    (void)type;
}

void numa_log_alloc(numa_log_t *nl, uint64_t info, arena_t *target_arena)
{
    (void)nl;
    (void)info;
    (void)target_arena;
}

void numa_log_flush(numa_log_t *nl)
{
    (void)nl;
}

void numa_log_slab_flusher(arena_t *arena)
{
    (void)arena;
}

void numa_log_extent_flusher(arena_t *arena)
{
    (void)arena;
}

void numa_log_recovery(void)
{
}

#endif /* PMALLOC_WBL */
