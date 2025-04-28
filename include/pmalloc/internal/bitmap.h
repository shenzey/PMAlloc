
#ifdef PMALLOC_H_TYPES
typedef struct bitmap_header_s bitmap_header_t;
typedef unsigned long bitmap_t;
typedef uint8_t metamap_t;

#endif /* PMALLOC_H_TYPES */
/******************************************************************************/
#ifdef PMALLOC_H_STRUCTS

struct bitmap_header_s
{
    uint16_t nbits;
    uint16_t cur;
};

#endif /* PMALLOC_H_STRUCTS */
/******************************************************************************/
#ifdef PMALLOC_H_EXTERNS



#endif /* PMALLOC_H_EXTERNS */
/******************************************************************************/
#ifdef PMALLOC_H_INLINES

static inline size_t
bitmap_ffu(const bitmap_t *bitmap, size_t min_bit)
{
    bitmap_t bm = *bitmap;
    bitmap_t tmp = bm >> (min_bit);
    assert(tmp != 0);
    return __builtin_ffsl(tmp) - 1 + min_bit;
}

static inline size_t
bitmap_sfu(int num, int nbits, bitmap_t *bitmap, bool do_set)
{
    size_t bit;
    bitmap_t g;
    unsigned i;

    i = 0;
    g = bitmap[0];
    while ((bit = __builtin_ffsl(g)) == 0)
    {
        i++;
        assert(i < (unsigned)num);
        g = bitmap[i];
    }
    if (do_set)
        bitmap[i] = g ^ (g & -g);

    return bit - 1 + i * 64;
}

static inline void bitmap_unset_bits(bitmap_t *bitmap, int index)
{
    int group_index = index / 64;
    int bit_index = index % 64;
    bitmap_t mask = 1ULL << bit_index;
    bitmap[group_index] |= mask;
    return;
}

static inline void bitmap_set_bits(bitmap_t *bitmap, int index)
{
    int group_index = index / 64;
    int bit_index = index % 64;
    bitmap_t mask = 1ULL << bit_index;
    bitmap[group_index] ^= mask;
    return;
}

static inline int get_bitmap_num(bitmap_t *bitmap, int num)
{
    int ret = 0;
    for (int i = 0; i < num; i++)
    {
        ret += __builtin_popcountll(bitmap[i]);
    }

    return 64 * num - ret;
}

#endif /* PMALLOC_H_INLINES */
       /******************************************************************************/