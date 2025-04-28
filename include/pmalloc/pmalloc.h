#ifndef PMALLOC_H_
#define PMALLOC_H_
#ifdef __cplusplus
extern "C"
{
#endif

#include "stdint.h"

    void *pmalloc_malloc_to(size_t size, void **ptr);

    void pmalloc_free_from(void **pptr);

    int pmalloc_init();

    int pmalloc_close();

    uint64_t pmget_memory_usage();

    void pmalloc_numa_migrate_check();

#ifdef __cplusplus
};
#endif
#endif /* PMALLOC_H_ */