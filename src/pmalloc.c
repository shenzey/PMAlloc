
#define PMALLOC_C_
#include "pmalloc/internal/pmalloc_internal.h"

/******************************************************************************/
/* Data. */

int32_t *logs;
struct stat sb;

size_t opt_narenas = 0;

unsigned ncpus;

pthread_mutex_t arenas_lock;
arena_t **arenas;
unsigned narenas_total;
unsigned narenas_auto;

sem_t **sems[2];
volatile unsigned char migrate_check;

/******************************************************************************/
/* Function prototypes for non-inline static functions. */

/******************************************************************************/
/* Inline tool function */

/******************************************************************************/
arena_t *
arenas_extend(unsigned ind)
{
    arena_t *ret;

    ret = (arena_t *)_malloc(sizeof(arena_t));
    if (ret != NULL && arena_new(ret, ind) == false)
    {
        arenas[ind] = ret;
        return (ret);
    }

    return (arenas[0]);
}

arena_t *
choose_arena_hard(void)
{

    arena_t *ret;

    if (narenas_auto > 1)
    {
        unsigned i, choose;
        choose = 0;
        pthread_mutex_lock(&arenas_lock);
        assert(arenas[0] != NULL);
        unsigned cpu = 0;
        unsigned node = 0;
        getcpu(&cpu,&node);
        choose = cpu;

        if (arenas[choose] != NULL){
            ret = arenas[choose];
        }
        else
        {
            ret = arenas_extend(choose);
        }
        ret->nthreads++;
        pthread_mutex_unlock(&arenas_lock); 
    }
    else
    {
        ret = arenas[0];
        pthread_mutex_lock(&arenas_lock);
        ret->nthreads++;
        pthread_mutex_unlock(&arenas_lock);
    }

    arenas_tsd_set(&ret);

    return (ret);
}

static unsigned
malloc_ncpus(void)
{
    long result;

    result = sysconf(_SC_NPROCESSORS_ONLN);

    return ((result == -1) ? 1 : (unsigned)result);
}

void arenas_cleanup(void *arg)
{
    arena_t *arena = *(arena_t **)arg;

    pthread_mutex_lock(&arenas_lock);
    arena->nthreads--;
    pthread_mutex_unlock(&arenas_lock);
}

void numa_migrate_check(tcache_t *tcache)
{
    unsigned cpu = 0;
    unsigned node = 0;
    getcpu(&cpu,&node);
    arena_t* arena = choose_arena(NULL);
    if(unlikely(arena->ind != cpu))
    {
        for(int sc = 0; sc < MAX_SZ_IDX; sc++)
        {
            cache_t *cache = &tcache->caches[sc];
            if(cache->bm == 0) continue;
            for(int cid = 0; cid < NBANKS; cid++)
            {
                for(int i = 0; i < cache->ncached[cid]; i++)
                {
                    void *ptr = cache->avail[cid][i].ret;
                    vslab_t *vslab = rtree_vslab_read(tcache, &extents_rtree, (uintptr_t)ptr);
                    migrate_putback(arena, tcache, vslab, ptr);
                }
                cache->ncached[cid] = 0;
            }
            cache->bm = 0;
        }

        pthread_mutex_lock(&arenas_lock);
        arena->nthreads--;
        pthread_mutex_unlock(&arenas_lock);
        arena = choose_arena_hard();
    }
    tcache->migrate_check = migrate_check;
}

static inline bool is_small_size(size_t size, size_t *idx)
{
    if (size > MAX_SZ)
    {
        return false;
    }
    else
    {
        *idx = get_sizeclass_id_by_size(size);

        if (unlikely(*idx == 32 || *idx == 36 || *idx == 38))
        {

            return false;
        }
    }
    return true;
}

void *pmalloc_malloc_to(size_t size, void **ptr)
{
    size = ALIGNMENT_CEILING(size, CACHE_LINE_SIZE);
    tcache_t *tcache = tcache_get();
    if(unlikely(ticker_tick(&tcache->numa_ticker) || migrate_check != tcache->migrate_check))
        numa_migrate_check(tcache);
    size_t sz_idx;
    if (is_small_size(size, &sz_idx))
    {
        cache_t *cache = &tcache->caches[sz_idx];
        void *ret;

#ifdef SLAB_MORPHING
        while (true)
        {
#endif
            if (unlikely(cache->bm == 0))
            {
                arena_t *arena = choose_arena(NULL);
                bin_t *bin = &arena->bins[sz_idx];
                assert(&bin->bin_lock);
                pthread_mutex_lock(&bin->bin_lock);
                tcache_fill_cache(arena, tcache, cache, bin, sz_idx);
                pthread_mutex_unlock(&bin->bin_lock);
            }

            int now = cache_bitmap_findnxt(cache->bm, cache->now);

#ifdef SLAB_MORPHING
            while (cache->ncached[now] >= 0 && cache->avail[now][cache->ncached[now]].is_sb)
            {
                cache->ncached[now]--;
            }

            if (cache->ncached[now] == -1)
            {
                cache->ncached[now] = 0;
                cache->bm = cache_bitmap_flip(cache->bm, now);
                continue;
            }
            else
            {
#endif // SLAB_MORPHING
                cache->ncached[now]--;
                cache_entry_t *entry = &cache->avail[now][cache->ncached[now]];
                arena_t* arena = choose_arena(NULL);
                add_minilog(minilog[arena->numa_ind], global_index + arena->numa_ind, ptr);

                meta_set(entry->metas, entry->index);
#ifdef SLAB_MORPHING
                dmeta_set_state(entry->dmeta, META_ALLOCED);
#endif
                persist_one(entry->metas);

                if (cache->ncached[now] == 0)
                {
                    cache->bm = cache_bitmap_flip(cache->bm, now);
                }
                cache->now = now;
                ret = entry->ret;
#ifdef PMALLOC_WBL
                wbl_dtt_track_alloc(&tcache->wbl_dtt, entry->vslab, ret, entry->metas, entry->index, entry->block_index);
                wbl_group_commit_maybe(tcache);
#endif
#ifdef SLAB_MORPHING
                break;
            }
        }
#endif // SLAB_MORPHING

        *ptr = ret;
        persist_one(ptr);
        return ret;
    }
    else
    {

        size_t npages = size % PAGE_SIZE == 0 ? size / PAGE_SIZE : size / PAGE_SIZE + 1;
        npages = get_psizeclass_by_idx(get_psizeclass(npages))->npages;
        void* ret = arena_large_alloc(tcache, choose_arena(NULL), npages, ptr);
        persist_one(ptr);

        return ret;
    }

    return NULL;
}

void pmalloc_free_from(void **pptr)

{

    void *ptr = *pptr;
    tcache_t *tcache = tcache_get();
    szind_t szind = MAX_PSZ_IDX;
    bool slab = false;
    rtree_szind_slab_read(&extents_rtree, &tcache->extents_rtree_ctx,
                          (uintptr_t)ptr, true, &szind, &slab);

    assert(szind != MAX_PSZ_IDX);
    if (slab)
    {
        assert(szind == SLAB_SCIND);
        vslab_t *vslab = rtree_vslab_read(tcache, &extents_rtree, (uintptr_t)ptr);
        arena_t *arena = NULL;
        arena = choose_arena(arena);
        assert(arena != NULL);

        slab_free_small(arena, tcache, vslab, ptr, pptr);
    }
    else
    {
        // We use the same write-ahead log(WAL) strategy as nvm_malloc. It doesn't use WAL in deallocating process.
        //  add_minilog(minilog, &global_index, pptr);


        assert(szind < MAX_PSZ_IDX);
        arena_dalloc(tcache, ptr, szind, slab);
        *pptr = NULL;
        persist_one(pptr);

        return;
    }
}

int directory_init(const char *path) {
    struct stat info;
    if(stat(path, &info) == 0 && S_ISDIR(info.st_mode))
        return 1;
    if(mkdir(path, 0666))
    {
        printf("PMem directory init fail\n");
        exit(1);
    }
    return 0;
}

int pmalloc_init()
{
    ncpus = malloc_ncpus();
    if (opt_narenas == 0)
    {
        if (ncpus > 1)
            opt_narenas = ncpus << 2;
        else
            opt_narenas = 1;
    }

    int dir0 = directory_init(PMEMPATH[0]);
    int dir1 = directory_init(PMEMPATH[1]);
#ifndef PMALLOC_WBL
    if(dir0 || dir1)
        numa_log_recovery();

    minilog[0] = minilog_create(0);
    minilog[1] = minilog_create(1);
    global_index[0] = global_index[1] = 0;
#else
    (void)dir0;
    (void)dir1;
    wbl_log_init();
#endif

    tcache_boot(opt_narenas);
    sizeclass_boot();

    if (extent_boot())
    {
        return -1;
    }

    if (rtree_new(&extents_rtree, true))
    {
        exit(1);
    }

    arena_t *init_arenas[1];
    sem_t *init_sems[2][1];
    arena_boot();

    narenas_total = narenas_auto = 1;
    arenas = init_arenas;
    sems[0] = init_sems[0];
    sems[1] = init_sems[1];
    memset(arenas, 0, sizeof(arena_t *) * narenas_auto);
    arenas_extend(0);

    
    narenas_auto = opt_narenas;
    narenas_total = narenas_auto;

    arenas = (arena_t **)_malloc(sizeof(arena_t *) * narenas_total);
    if (arenas == NULL)
    {
        assert(false);
    }

    memset(arenas, 0, sizeof(arena_t *) * narenas_total);
    arenas[0] = init_arenas[0];

    sems[0] = (sem_t **)_malloc(sizeof(sem_t *) * narenas_total);
    sems[0][0] = init_sems[0][0];
    sems[1] = (sem_t **)_malloc(sizeof(sem_t *) * narenas_total);
    sems[1][0] = init_sems[1][0];

    return 0;
}

int pmalloc_close()
{
#ifndef PMALLOC_WBL
    for (int k = 0; k < narenas_total; k++)
    {
        arena_t *arena = arenas[k];
        if (arena == NULL)
            break;
        pthread_cancel(arena->slab_flusher);
        pthread_cancel(arena->extent_flusher);
        pthread_cancel(arena->log_GC);
    }
#endif
    return 0;
}

uint64_t pmget_memory_usage()
{
    uint64_t usage = 0;
    struct stat statbuf;
    char fname[100];
    for (int k = 0; k < narenas_total; k++)
    {
        arena_t *arena = arenas[k];
        if (arena == NULL)
            break;
        for (int i = 0; i < arena->file_id; i++)
        {
            get_filepath(arena->ind, fname, i, PMEMPATH[arena->numa_ind]);
            if (stat(fname, &statbuf) == -1)
            {
                printf("%s stat error! errno = %d\n\n", fname, errno);
                exit(1);
            }
            usage += (uint64_t)statbuf.st_blocks * 512;
        }
    }
    return usage;
}

void pmalloc_numa_migrate_check()
{
    migrate_check++;
}