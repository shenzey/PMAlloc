#include "pmalloc/internal/pmalloc_internal.h"

#ifdef PMALLOC_WBL

#include <sys/stat.h>

#define WBL_LOG_FILENAME "wbl_gap.log"

static int wbl_log_fd = -1;
static char wbl_log_path[PATH_MAX];
static pthread_mutex_t wbl_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t wbl_log_once = PTHREAD_ONCE_INIT;
static size_t wbl_log_max_bytes = 4 * 1024 * 1024;
static size_t wbl_log_tail_records = 64;

static void
wbl_log_load_config(void)
{
    const char *env = getenv("PMALLOC_WBL_LOG_MAX_BYTES");
    if (env != NULL) {
        char *endptr = NULL;
        unsigned long long val = strtoull(env, &endptr, 10);
        if (endptr != env) {
            if (val == 0) {
                wbl_log_max_bytes = 0;
            } else if (val >= sizeof(wbl_gap_rec_t) * 2) {
                wbl_log_max_bytes = (size_t)val;
            }
        }
    }

    env = getenv("PMALLOC_WBL_LOG_TAIL_RECORDS");
    if (env != NULL) {
        char *endptr = NULL;
        unsigned long long val = strtoull(env, &endptr, 10);
        if (endptr != env && val >= 2) {
            wbl_log_tail_records = (size_t)val;
        }
    }

    if (wbl_log_tail_records < 2) {
        wbl_log_tail_records = 2;
    }
    if ((wbl_log_tail_records & 1ULL) != 0) {
        wbl_log_tail_records++;
    }
}

static uint64_t
wbl_crc(uint64_t cp, uint64_t cd)
{
    return cp ^ cd ^ 0x9E3779B97F4A7C15ull;
}

static const char *
wbl_log_default_directory(void)
{
    struct stat sbuf;
    if (stat(PMEMPATH[0], &sbuf) == 0 && S_ISDIR(sbuf.st_mode)) {
        return PMEMPATH[0];
    }
    return "/tmp/";
}

static int
wbl_log_open_locked(void)
{
    pthread_once(&wbl_log_once, wbl_log_load_config);

    if (wbl_log_fd != -1) {
        return 0;
    }

    const char *dir = wbl_log_default_directory();
    snprintf(wbl_log_path, sizeof(wbl_log_path), "%s%s", dir, WBL_LOG_FILENAME);

    wbl_log_fd = open(wbl_log_path, O_CREAT | O_RDWR | O_APPEND, 0644);
    if (wbl_log_fd == -1) {
        return -1;
    }

    return 0;
}

static void
wbl_log_compact_locked(void)
{
    if (wbl_log_fd == -1) {
        return;
    }

    if (wbl_log_max_bytes == 0) {
        return;
    }

    off_t end = lseek(wbl_log_fd, 0, SEEK_END);
    if (end <= (off_t)wbl_log_max_bytes) {
        return;
    }

    size_t record_size = sizeof(wbl_gap_rec_t);
    size_t total_records = (size_t)(end / (off_t)record_size);
    if (total_records == 0) {
        return;
    }

    size_t keep_records = wbl_log_tail_records;
    if (keep_records > total_records) {
        keep_records = total_records;
    }

    size_t max_records_by_bytes = wbl_log_max_bytes / record_size;
    if (max_records_by_bytes < 2) {
        max_records_by_bytes = 2;
    }
    if (keep_records > max_records_by_bytes) {
        keep_records = max_records_by_bytes;
    }

    if ((keep_records & 1ULL) != 0) {
        if (keep_records > 2) {
            keep_records--;
        } else {
            keep_records = 2;
        }
    }

    size_t keep_bytes = keep_records * record_size;
    off_t start = end - (off_t)keep_bytes;
    if (lseek(wbl_log_fd, start, SEEK_SET) == (off_t)-1) {
        return;
    }

    wbl_gap_rec_t *buffer = (wbl_gap_rec_t *)_malloc(keep_bytes);
    if (buffer == NULL) {
        return;
    }

    ssize_t rd = read(wbl_log_fd, buffer, keep_bytes);
    if (rd != (ssize_t)keep_bytes) {
        _free(buffer);
        return;
    }

    if (ftruncate(wbl_log_fd, 0) != 0) {
        _free(buffer);
        return;
    }

    ssize_t wr = write(wbl_log_fd, buffer, keep_bytes);
    _free(buffer);
    if (wr != (ssize_t)keep_bytes) {
        return;
    }

    _mm_sfence();
}

int
wbl_log_init(void)
{
    pthread_mutex_lock(&wbl_log_mutex);
    int ret = wbl_log_open_locked();
    pthread_mutex_unlock(&wbl_log_mutex);
    return ret;
}

static int
wbl_log_append(const wbl_gap_rec_t *rec)
{
    ssize_t written = write(wbl_log_fd, rec, sizeof(*rec));
    if (written != (ssize_t)sizeof(*rec)) {
        return -1;
    }
    _mm_sfence();
    return 0;
}

int
wbl_log_begin_round(uint64_t *out_cp)
{
    if (out_cp == NULL) {
        return -1;
    }

    pthread_mutex_lock(&wbl_log_mutex);
    if (wbl_log_open_locked() != 0) {
        pthread_mutex_unlock(&wbl_log_mutex);
        return -1;
    }

    uint64_t cp = numa_timestamp();
    wbl_gap_rec_t rec = {
        .cp = cp,
        .cd = 0,
        .crc = wbl_crc(cp, 0),
    };
    int rc = wbl_log_append(&rec);
    if (rc == 0) {
        *out_cp = cp;
    }
    pthread_mutex_unlock(&wbl_log_mutex);
    return rc;
}

int
wbl_log_end_round(uint64_t cp, uint64_t cd)
{
    pthread_mutex_lock(&wbl_log_mutex);
    if (wbl_log_open_locked() != 0) {
        pthread_mutex_unlock(&wbl_log_mutex);
        return -1;
    }

    wbl_gap_rec_t rec = {
        .cp = cp,
        .cd = cd,
        .crc = wbl_crc(cp, cd),
    };
    int rc = wbl_log_append(&rec);
    if (rc == 0) {
        wbl_log_compact_locked();
    }
    pthread_mutex_unlock(&wbl_log_mutex);
    return rc;
}

int
wbl_log_read_latest(uint64_t *cp, uint64_t *cd)
{
    pthread_mutex_lock(&wbl_log_mutex);
    if (wbl_log_open_locked() != 0) {
        pthread_mutex_unlock(&wbl_log_mutex);
        return -1;
    }

    off_t end = lseek(wbl_log_fd, 0, SEEK_END);
    if (end < (off_t)sizeof(wbl_gap_rec_t)) {
        pthread_mutex_unlock(&wbl_log_mutex);
        return -1;
    }

    if (lseek(wbl_log_fd, end - sizeof(wbl_gap_rec_t), SEEK_SET) == (off_t)-1) {
        pthread_mutex_unlock(&wbl_log_mutex);
        return -1;
    }

    wbl_gap_rec_t rec;
    ssize_t rd = read(wbl_log_fd, &rec, sizeof(rec));
    if (rd != (ssize_t)sizeof(rec)) {
        pthread_mutex_unlock(&wbl_log_mutex);
        return -1;
    }

    if (rec.crc != wbl_crc(rec.cp, rec.cd)) {
        pthread_mutex_unlock(&wbl_log_mutex);
        return -1;
    }

    if (cp != NULL) {
        *cp = rec.cp;
    }
    if (cd != NULL) {
        *cd = rec.cd;
    }

    pthread_mutex_unlock(&wbl_log_mutex);
    return 0;
}

#endif /* PMALLOC_WBL */
