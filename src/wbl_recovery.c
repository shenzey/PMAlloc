#include "pmalloc/internal/pmalloc_internal.h"

#ifdef PMALLOC_WBL

wbl_recovery_status_t
wbl_recover_vslab(vslab_t *vslab)
{
    (void)vslab;
    return WBL_RECOVERY_STATUS_UNCHANGED;
}

#endif /* PMALLOC_WBL */
