#ifndef PMALLOC_WBL_DTT_H
#define PMALLOC_WBL_DTT_H

#include <stddef.h>
#include <stdint.h>

#ifdef PMALLOC_WBL

typedef struct vslab_s vslab_t;
typedef struct tcache_s tcache_t;

typedef enum {
    WBL_DTT_ENTRY_ALLOC = 0,
    WBL_DTT_ENTRY_FREE_INTENT = 1
} wbl_dtt_entry_type_t;

typedef struct {
    wbl_dtt_entry_type_t type;
    vslab_t *vslab;
    void *ptr;
    void *meta;
    unsigned meta_bit;
    uint32_t block_index;
} wbl_dtt_entry_t;

typedef struct {
    wbl_dtt_entry_t *entries;
    size_t count;
    size_t capacity;
    uint64_t last_commit_ts;
    uint64_t first_pending_ts;
} wbl_dtt_t;

void wbl_dtt_init(wbl_dtt_t *dtt);
void wbl_dtt_fini(wbl_dtt_t *dtt);
void wbl_dtt_reset(wbl_dtt_t *dtt);
void wbl_dtt_track_alloc(wbl_dtt_t *dtt, vslab_t *vslab, void *ptr, void *meta, unsigned meta_bit, uint32_t block_index);
void wbl_dtt_track_free_intent(wbl_dtt_t *dtt, vslab_t *vslab, void *ptr, void *meta, unsigned meta_bit, uint32_t block_index);

#endif /* PMALLOC_WBL */

#endif /* PMALLOC_WBL_DTT_H */
