#include "pmalloc/internal/pmalloc_internal.h"

#ifdef PMALLOC_WBL

#define WBL_DTT_INITIAL_CAPACITY 32

static bool
wbl_dtt_reserve(wbl_dtt_t *dtt, size_t additional)
{
    if (dtt->count + additional <= dtt->capacity) {
        return true;
    }

    size_t new_capacity = dtt->capacity ? dtt->capacity : WBL_DTT_INITIAL_CAPACITY;
    while (new_capacity < dtt->count + additional) {
        new_capacity *= 2;
    }

    wbl_dtt_entry_t *entries = (wbl_dtt_entry_t *)_malloc(new_capacity * sizeof(wbl_dtt_entry_t));
    if (entries == NULL) {
        return false;
    }

    if (dtt->entries != NULL && dtt->count > 0) {
        memcpy(entries, dtt->entries, dtt->count * sizeof(wbl_dtt_entry_t));
    }

    if (dtt->entries != NULL) {
        _free(dtt->entries);
    }

    dtt->entries = entries;
    dtt->capacity = new_capacity;
    return true;
}

static void
wbl_dtt_append(wbl_dtt_t *dtt, const wbl_dtt_entry_t *entry)
{
    if (!wbl_dtt_reserve(dtt, 1)) {
        return;
    }

    dtt->entries[dtt->count++] = *entry;
}

void
wbl_dtt_init(wbl_dtt_t *dtt)
{
    dtt->entries = NULL;
    dtt->count = 0;
    dtt->capacity = 0;
    dtt->last_commit_ts = numa_timestamp();
    dtt->first_pending_ts = 0;
}

void
wbl_dtt_fini(wbl_dtt_t *dtt)
{
    if (dtt->entries != NULL) {
        _free(dtt->entries);
        dtt->entries = NULL;
    }
    dtt->count = 0;
    dtt->capacity = 0;
    dtt->last_commit_ts = 0;
    dtt->first_pending_ts = 0;
}

void
wbl_dtt_reset(wbl_dtt_t *dtt)
{
    dtt->count = 0;
    dtt->first_pending_ts = 0;
}

void
wbl_dtt_track_alloc(wbl_dtt_t *dtt, vslab_t *vslab, void *ptr, void *meta, unsigned meta_bit, uint32_t block_index)
{
    if (dtt == NULL) {
        return;
    }

    if (dtt->count == 0) {
        dtt->first_pending_ts = numa_timestamp();
    }

    wbl_dtt_entry_t entry = {
        .type = WBL_DTT_ENTRY_ALLOC,
        .vslab = vslab,
        .ptr = ptr,
        .meta = meta,
        .meta_bit = meta_bit,
        .block_index = block_index,
    };
    wbl_dtt_append(dtt, &entry);
}

void
wbl_dtt_track_free_intent(wbl_dtt_t *dtt, vslab_t *vslab, void *ptr, void *meta, unsigned meta_bit, uint32_t block_index)
{
    if (dtt == NULL) {
        return;
    }

    if (dtt->count == 0) {
        dtt->first_pending_ts = numa_timestamp();
    }

    wbl_dtt_entry_t entry = {
        .type = WBL_DTT_ENTRY_FREE_INTENT,
        .vslab = vslab,
        .ptr = ptr,
        .meta = meta,
        .meta_bit = meta_bit,
        .block_index = block_index,
    };
    wbl_dtt_append(dtt, &entry);
}

#endif /* PMALLOC_WBL */
