#ifndef _OMRX_INTERNAL_H
#define _OMRX_INTERNAL_H

#include "omrx.h"

/** @cond internal
  */

#define OMRX_ERRMSG_BUFSIZE 4096

typedef struct omrx_attr *omrx_attr_t;

//FIXME: make this a hashtable or something
struct idmap_st {
    const char *id;
    omrx_chunk_t chunk;
};

struct omrx {
    FILE *fp;
    char *filename;
    bool close_file;
    char *message;
    omrx_log_func_t log_error;
    omrx_log_func_t log_warning;
    omrx_alloc_func_t alloc;
    omrx_free_func_t free;
    struct omrx_chunk *root_chunk;
    struct omrx_chunk *context;
    struct idmap_st *chunk_id_map;
    size_t chunk_id_map_size;
    omrx_status_t status;
    omrx_status_t last_result;
    void *user_data;
};

struct omrx_chunk {
    struct omrx_chunk *next;
    struct omrx_chunk *parent;
    struct omrx_chunk *first_child;
    struct omrx_chunk *last_child;
    struct omrx *omrx;
    uint8_t tag[5];
    uint32_t tagint;
    uint16_t attr_count;
    struct omrx_attr *attrs;
    char *id;
    off_t file_position;
};

struct omrx_attr {
    struct omrx_attr *next;
    struct omrx_chunk *chunk;
    uint16_t id;
    uint16_t datatype;
    uint32_t size;
    off_t file_pos;
    void *data;
    uint16_t cols;
};

#define TAG_TO_TAGINT(t) ((((t)[0] & 0xff) << 24) | (((t)[1] & 0xff) << 16) | (((t)[2] & 0xff) << 8) | ((t)[3] & 0xff))

#define CHUNK_TAG_FLAG 0x20
#define ANCILLARY_CHUNK_FLAG 0x20000000
#define COPYABLE_CHUNK_FLAG  0x00200000
#define END_CHUNK_FLAG       0x00000020

// FIXME: make this correct based on detected machine endianness
#define UINT16_FTOH(value) (value)
#define UINT32_FTOH(value) (value)
#define UINT16_HTOF(value) (value)
#define UINT32_HTOF(value) (value)

#define CHECK_ALLOC(omrx, x) if ((x) == NULL) { return omrx_os_error((omrx), OMRX_ERR_ALLOC, "Memory allocation failed"); }
#define CHECK_ERR(x) do { omrx_status_t __x = (x); if (__x < 0) return __x; } while (0);
#define CHECK_OK(x) do { omrx_status_t __x = (x); if (__x != OMRX_STATUS_OK) return __x; } while (0);
#define API_RESULT(omrx, x) (omrx->last_result = (x))

/** @endcond */

#endif /* _OMRX_INTERNAL_H */
