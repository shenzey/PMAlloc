#define PMALLOC_NUMA_LOG_C_
#include "pmalloc/internal/pmalloc_internal.h"

/******************************************************************************/
/* Data. */

/******************************************************************************/
/* Function prototypes for non-inline static functions. */

/******************************************************************************/
/* Inline tool function */

static inline void *numa_log_file_create(unsigned numa_ind, uint id)
{
    void *addr, *tmp;

    size_t mapped_len;
    char str[100];
    int is_pmem;
    sprintf(str, "%snumalog_%x_%d", PMEMPATH[numa_ind], pthread_self(), id);

    if ((tmp = pmem_map_file(str, NUMA_LOG_FILE_SIZE, PMEM_FILE_CREATE | PMEM_FILE_SPARSE, 0666, &mapped_len, &is_pmem)) == NULL)
    {
        printf("map file fail!\n %d\n", errno);
        exit(1);
    }
    if (!is_pmem)
    {
        printf("is not nvm!\n");
        exit(1);
    }

    return tmp;
}

#ifdef NUMA_LOG_IM
static inline int numa_bid_to_mid(int bid)
{

    int n = NUMA_LOG_LOG_ENTRY_NUM_PER_CACHE_LINE;
    int nn = n * NUMA_LOG_NBANKS;
    int i = bid % nn;
    return (bid / nn) * nn + (bid % NBANKS) * n + i / NBANKS;
}
#endif

/******************************************************************************/

void numa_log_init(numa_log_t *nl, arena_t *arena, uint8_t type)
{

#ifdef NUMA_LOG_IM
    nl->nle_id[0] = nl->nle_id[1] = 0;
#else
    nl->nle_offset[0] = nl->nle_offset[1] = 0;
#endif
    nl->file_ptr[0] = (numa_log_entry_t *)numa_log_file_create(arena->numa_ind, (type << 1) + 0);
    nl->file_ptr[1] = (numa_log_entry_t *)numa_log_file_create(arena->numa_ind, (type << 1) + 1);
    nl->limit = NUMA_LOG_FILE_SIZE;
    nl->turn = 0;
    nl->nbitmaps = ((narenas_total % 64 ? 64 - narenas_total % 64 : 0) + narenas_total) / 64;
    nl->arenas_used = (bitmap_t *)_malloc(nl->nbitmaps * sizeof(bitmap_t));
    memset(nl->arenas_used, 0, nl->nbitmaps  * sizeof(bitmap_t));
    nl->type = type;
}

void numa_log_alloc(numa_log_t *nl, uint64_t info, arena_t *target_arena)
{
#ifdef NUMA_LOG_IM
    if (nl->nle_id[nl->turn] == NUMA_LOG_FILE_SIZE / sizeof(numa_log_entry_t))
#else
    if (nl->nle_offset[nl->turn] == nl->limit)
#endif
        numa_log_flush(nl);

#ifdef NUMA_LOG_IM
    numa_log_entry_t *nle = nl->file_ptr[nl->turn] + numa_bid_to_mid(nl->nle_id[nl->turn]);
#else
    numa_log_entry_t *nle = (numa_log_entry_t *)(((uintptr_t)nl->file_ptr[nl->turn]) + nl->nle_offset[nl->turn]);
#endif

    nle->info = info;
    nle->timestamp = numa_timestamp();
    persist_one(nle);
    assert(target_arena == arenas[target_arena->ind]);
    bitmap_unset_bits(nl->arenas_used, target_arena->ind);

#ifdef NUMA_LOG_IM
    nl->nle_id[nl->turn]++;
#else
    nl->nle_offset[nl->turn] += sizeof(numa_log_entry_t);
    _mm_prefetch(((uintptr_t)nle) + sizeof(numa_log_entry_t), _MM_HINT_T0);
#endif
}

void numa_log_flush(numa_log_t *nl)
{
    nl->turn ^= 1;
#ifdef NUMA_LOG_IM
    nl->nle_id[nl->turn] = 0;
#else
    nl->nle_offset[nl->turn] = 0;
#endif
    for (int i = 0; i < nl->nbitmaps; i++)
        while (nl->arenas_used[i])
        {
            size_t bit = __builtin_ffsl(nl->arenas_used[i]);
            sem_post(sems[nl->type][bit - 1 + i * 64]);
            nl->arenas_used[i] ^= (nl->arenas_used[i] & -nl->arenas_used[i]);
        }
}

void numa_log_slab_flusher(arena_t *arena)
{
    while (1)
    {
        sem_wait(&(arena->slab_sem));
        pthread_mutex_lock(&arena->numa_lock);
        while (ql_first(&arena->numa_list))
        {
            vslab_t *vslab = ql_first(&arena->numa_list);
            if (vslab->r_dirty)
            {
                pthread_mutex_lock(&vslab->slab_lock);

                if (vslab->r_dirty)
                {
                    metamap_t *temp_meta = (metamap_t *)_malloc(vslab->sc.roffset - vslab->sc.moffset);
                    memset(temp_meta, 0, vslab->sc.roffset - vslab->sc.moffset);

#if NBANKS == 1 // interleave mapping not used
                    for (int i = 0; i < vslab->sc.n_bitmaps; i++)
                        if (vslab->rbitmap[i] < 0xffffffffffffffffull)
                        {
                            bitmap_t rev = ~vslab->rbitmap[i];
                            vslab->bitmap[i] |= rev;
                            ((bitmap_t *)temp_meta)[i] |= rev;
                            vslab->rbitmap[i] = 0xffffffffffffffffull;
                            vslab->bitmap_header.nbits -= __builtin_popcountl(rev);
                        }
#else
                    size_t bit;
                    for (int i = 0; i < vslab->sc.n_bitmaps; i++)
                        if (vslab->rbitmap[i] < 0xffffffffffffffffull)
                        {
                            bitmap_t rev = ~vslab->rbitmap[i];
                            vslab->bitmap[i] |= rev;
                            while (bit = __builtin_ffsl(rev))
                            {
                                int mid = bid_to_mid(bit - 1 + 64 * i);
                                temp_meta[mid / 8] |= (1 << (mid % 8));
                                rev -= (1ull << (bit - 1));
                                vslab->bitmap_header.nbits--;
                            }
                            vslab->rbitmap[i] = 0xffffffffffffffffull;
                        }
#endif
                    uintptr_t cur_cacheline = 0;
                    for (int i = 0; i < vslab->sc.roffset - vslab->sc.moffset; i++)
                        if (temp_meta[i])
                        {
                            __sync_fetch_and_and((metamap_t *)((uintptr_t)vslab->slab + vslab->sc.moffset + i), ~temp_meta[i]);
                            if (((uintptr_t)vslab->slab + vslab->sc.moffset + i) / CACHE_LINE_SIZE != cur_cacheline)
                            {
                                if (cur_cacheline)
                                    persist_one((void *)(cur_cacheline * CACHE_LINE_SIZE));
                                cur_cacheline = ((uintptr_t)vslab->slab + vslab->sc.moffset + i) / CACHE_LINE_SIZE;
                            }
                        }
                    if (cur_cacheline > 0)
                        persist_one((void *)(cur_cacheline * CACHE_LINE_SIZE));

                    vslab->slab->numa_timestamp = numa_timestamp();
                    persist_one(&vslab->slab->numa_timestamp);

                    vslab->r_dirty = 0;
                    _free(temp_meta);
                }
                pthread_mutex_unlock(&vslab->slab_lock);
            }

            ql_first(&arena->numa_list) = qr_next(ql_first(&arena->numa_list), numa_link);
            if (ql_first(&arena->numa_list) == vslab)
                ql_first(&arena->numa_list) = NULL;
            else
                qr_remove(vslab, numa_link);
        }
        pthread_mutex_unlock(&arena->numa_lock);
    }
}


void numa_log_extent_flusher(arena_t *arena)
{
    while(1)
    {
        sem_wait(&(arena->extent_sem));
        pthread_mutex_lock(&arena->log_lock);
        shadow_chunk_t* shadow_ptr = arena->shadow_tomb->next;
        while(shadow_ptr)
        {
            for(int i = 0; i < shadow_ptr->cnt; i++)
                flush_tomb(arena, &arena->log, shadow_ptr->items[i]);
            shadow_chunk_t* p = shadow_ptr->next;
            _free(shadow_ptr);
            shadow_ptr = p;
        }
        shadow_ptr = arena->shadow_tomb;
        for(int i = 0; i < shadow_ptr->cnt; i++)
            flush_tomb(arena, &arena->log, shadow_ptr->items[i]);
        log_file_head_t* head = (log_file_head_t*)arena->log->log_file_addr;
        head->timestamp = numa_timestamp();
        persist_one(&head->timestamp);
        arena->shadow_tomb->next = NULL;
        arena->shadow_tomb->cnt = 0;
        pthread_mutex_unlock(&arena->log_lock);
    }
}

void numa_log_recovery() {}