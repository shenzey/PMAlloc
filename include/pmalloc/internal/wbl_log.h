#ifndef PMALLOC_WBL_LOG_H
#define PMALLOC_WBL_LOG_H

#include <stdint.h>

#ifdef PMALLOC_WBL

typedef struct {
    uint64_t cp;
    uint64_t cd;
    uint64_t crc;
} wbl_gap_rec_t;

int wbl_log_init(void);
int wbl_log_begin_round(uint64_t *out_cp);
int wbl_log_end_round(uint64_t cp, uint64_t cd);
int wbl_log_read_latest(uint64_t *cp, uint64_t *cd);

#endif /* PMALLOC_WBL */

#endif /* PMALLOC_WBL_LOG_H */
