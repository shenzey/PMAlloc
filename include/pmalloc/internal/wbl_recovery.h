#ifndef PMALLOC_WBL_RECOVERY_H
#define PMALLOC_WBL_RECOVERY_H

#ifdef PMALLOC_WBL

typedef struct vslab_s vslab_t;

typedef enum {
    WBL_RECOVERY_STATUS_OK = 0,
    WBL_RECOVERY_STATUS_UNCHANGED = 1,
    WBL_RECOVERY_STATUS_ERROR = -1
} wbl_recovery_status_t;

wbl_recovery_status_t wbl_recover_vslab(vslab_t *vslab);

#endif /* PMALLOC_WBL */

#endif /* PMALLOC_WBL_RECOVERY_H */
