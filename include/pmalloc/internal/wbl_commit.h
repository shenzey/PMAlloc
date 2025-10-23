#ifndef PMALLOC_WBL_COMMIT_H
#define PMALLOC_WBL_COMMIT_H

#ifdef PMALLOC_WBL

typedef struct tcache_s tcache_t;

int wbl_group_commit_thread_local(tcache_t *tcache);
int wbl_group_commit_maybe(tcache_t *tcache);

#endif /* PMALLOC_WBL */

#endif /* PMALLOC_WBL_COMMIT_H */
