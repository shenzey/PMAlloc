#include "pmalloc/pmalloc.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    if (pmalloc_init() != 0) {
        fprintf(stderr, "pmalloc_init failed\n");
        return 1;
    }

    void *ptr = NULL;
    void *mem = pmalloc_malloc_to(64, &ptr);
    assert(mem != NULL);
    assert(ptr == mem);

    pmalloc_free_from(&ptr);
    assert(ptr == NULL);

    if (pmalloc_close() != 0) {
        fprintf(stderr, "pmalloc_close failed\n");
        return 1;
    }

    return 0;
}
