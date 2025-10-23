#include "pmalloc/internal/pmalloc_internal.h"

#ifdef PMALLOC_WBL

#include "pmalloc/internal/wbl_log.h"
#include "pmalloc/internal/wbl_dtt.h"

static pthread_once_t wbl_commit_once = PTHREAD_ONCE_INIT;
static size_t wbl_commit_soft_ops = 32;
static size_t wbl_commit_hard_ops = 256;
static uint64_t wbl_commit_delay_ns = 5 * 1000 * 1000;

static void
wbl_commit_load_config(void)
{
    const char *env = getenv("PMALLOC_WBL_COMMIT_OPS");
    if (env != NULL) {
        char *endptr = NULL;
        unsigned long val = strtoul(env, &endptr, 10);
        if (endptr != env) {
            wbl_commit_soft_ops = val;
        }
    }

    env = getenv("PMALLOC_WBL_COMMIT_MAX_OPS");
    if (env != NULL) {
        char *endptr = NULL;
        unsigned long val = strtoul(env, &endptr, 10);
        if (endptr != env && val > 0) {
            wbl_commit_hard_ops = val;
        }
    }

    env = getenv("PMALLOC_WBL_COMMIT_NS");
    if (env != NULL) {
        char *endptr = NULL;
        unsigned long long val = strtoull(env, &endptr, 10);
        if (endptr != env) {
            wbl_commit_delay_ns = val;
        }
    }

    if (wbl_commit_soft_ops == 0) {
        wbl_commit_soft_ops = 0;
    }

    if (wbl_commit_hard_ops == 0) {
        wbl_commit_hard_ops = wbl_commit_soft_ops > 0 ? wbl_commit_soft_ops : 1;
    }

    if (wbl_commit_hard_ops < wbl_commit_soft_ops && wbl_commit_soft_ops > 0) {
        wbl_commit_hard_ops = wbl_commit_soft_ops;
    }
}

static void
wbl_commit_entry(const wbl_dtt_entry_t *entry)
{
    if (entry->meta != NULL) {
        persist_one(entry->meta);
    }
    if (entry->ptr != NULL) {
        persist_one(&entry->ptr);
    }
    (void)entry;
}

int
wbl_group_commit_thread_local(tcache_t *tcache)
{
    if (tcache == NULL) {
        return 0;
    }

    pthread_once(&wbl_commit_once, wbl_commit_load_config);

    wbl_dtt_t *dtt = &tcache->wbl_dtt;
    if (dtt->count == 0) {
        return 0;
    }

    uint64_t cp = 0;
    if (wbl_log_begin_round(&cp) != 0) {
        return -1;
    }

    for (size_t i = 0; i < dtt->count; ++i) {
        wbl_commit_entry(&dtt->entries[i]);
    }

    uint64_t cd = numa_timestamp();
    wbl_log_end_round(cp, cd);
    wbl_dtt_reset(dtt);
    dtt->last_commit_ts = cd;
    return 0;
}

int
wbl_group_commit_maybe(tcache_t *tcache)
{
    if (tcache == NULL) {
        return 0;
    }

    pthread_once(&wbl_commit_once, wbl_commit_load_config);

    wbl_dtt_t *dtt = &tcache->wbl_dtt;
    if (dtt->count == 0) {
        return 0;
    }

    if (wbl_commit_hard_ops > 0 && dtt->count >= wbl_commit_hard_ops) {
        return wbl_group_commit_thread_local(tcache);
    }

    if (wbl_commit_soft_ops > 0 && dtt->count >= wbl_commit_soft_ops) {
        return wbl_group_commit_thread_local(tcache);
    }

    if (wbl_commit_delay_ns > 0) {
        uint64_t now = numa_timestamp();
        uint64_t reference = dtt->first_pending_ts != 0 ? dtt->first_pending_ts : dtt->last_commit_ts;
        if (now - reference >= wbl_commit_delay_ns) {
            return wbl_group_commit_thread_local(tcache);
        }
    }

    return 0;
}

#endif /* PMALLOC_WBL */
