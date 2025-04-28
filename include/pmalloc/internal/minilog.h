/******************************************************************************/
#ifdef PMALLOC_H_TYPES

typedef struct mini_item_s mini_item_t;
typedef struct minilog_s minilog_t;
#define MINILOG_ITEM_SIZE (sizeof(mini_item_t))
#define MINILOG_SIZE (sizeof(minilog_t))
#define MINILOG_NUM (NBANKS * 2 * (CACHE_LINE_SIZE / MINILOG_ITEM_SIZE)) // 2 because we have 2 DIMMs, we want the WAL to use all DIMMs

#define MINILOG_TYPE_SMALL_ALLOC 1
#define MINILOG_TYPE_SMALL_FREE 2
#define MINILOG_TYPE_LARGE_ALLOC 3
#define MINILOG_TYPE_LARGE_FREE 4

#endif /* PMALLOC_H_TYPES */
/******************************************************************************/
#ifdef PMALLOC_H_STRUCTS

struct mini_item_s
{
   uint64_t ptr;
};

struct minilog_s
{
   mini_item_t log_item[MINILOG_NUM];
};

#endif /* PMALLOC_H_STRUCTS */
/******************************************************************************/
#ifdef PMALLOC_H_EXTERNS

extern minilog_t *minilog[2];
extern uint64_t global_index[2];

minilog_t *minilog_create(unsigned numa_ind);
void add_minilog(minilog_t *log, uint64_t *global_index, uint64_t ptr);

#endif /* PMALLOC_H_EXTERNS */
