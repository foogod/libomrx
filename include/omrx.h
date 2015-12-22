#ifndef _OMRX_H
#define _OMRX_H

#include <stdint.h>
#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef int omrx_status_t;
typedef enum {
    OMRX_OWN,
    OMRX_COPY,
} omrx_ownership_t;
typedef struct omrx *omrx_t;
typedef struct omrx_chunk *omrx_chunk_t;

typedef void (*omrx_log_func_t)(omrx_t omrx, omrx_status_t errcode, const char *msg);

#define OMRX_STATUS_OK        0
#define OMRX_STATUS_NOT_FOUND 1
#define OMRX_STATUS_DUP       2
#define OMRX_STATUS_NO_OBJECT 3

#define OMRX_OK OMRX_STATUS_OK

#define OMRX_WARNING        0x1000
#define OMRX_WARN_BAD_VER   (OMRX_WARNING + 0)
#define OMRX_WARN_BAD_ATTR  (OMRX_WARNING + 1)
#define OMRX_WARN_OSERR     (OMRX_WARNING + 2)

#define OMRX_ERR_BADAPI       -1
#define OMRX_ERR_OSERR        -2
#define OMRX_ERR_ALLOC        -3
#define OMRX_ERR_EOF          -4
#define OMRX_ERR_NOT_OPEN     -5
#define OMRX_ERR_ALREADY_OPEN -6
#define OMRX_ERR_BAD_MAGIC    -7
#define OMRX_ERR_BAD_VER      -8
#define OMRX_ERR_BAD_CHUNK    -9
#define OMRX_ERR_BAD_DTYPE    -10
#define OMRX_ERR_NODATA       -11

#define OMRX_TYPEF_UNSIGNED 0x0000
#define OMRX_TYPEF_SIGNED   0x0004
#define OMRX_TYPEF_FLOAT    0x0008
#define OMRX_TYPEF_SIMPLE   0x0000
#define OMRX_TYPEF_ARRAY    0x1000
#define OMRX_TYPEF_OTHER    0xf000

#define OMRX_DTYPE_U8         (OMRX_TYPEF_SIMPLE | OMRX_TYPEF_UNSIGNED | 0)
#define OMRX_DTYPE_S8         (OMRX_TYPEF_SIMPLE | OMRX_TYPEF_SIGNED   | 0)
#define OMRX_DTYPE_U16        (OMRX_TYPEF_SIMPLE | OMRX_TYPEF_UNSIGNED | 1)
#define OMRX_DTYPE_S16        (OMRX_TYPEF_SIMPLE | OMRX_TYPEF_SIGNED   | 1)
#define OMRX_DTYPE_U32        (OMRX_TYPEF_SIMPLE | OMRX_TYPEF_UNSIGNED | 2)
#define OMRX_DTYPE_S32        (OMRX_TYPEF_SIMPLE | OMRX_TYPEF_SIGNED   | 2)
#define OMRX_DTYPE_F32        (OMRX_TYPEF_SIMPLE | OMRX_TYPEF_FLOAT    | 2)
#define OMRX_DTYPE_U64        (OMRX_TYPEF_SIMPLE | OMRX_TYPEF_UNSIGNED | 3)
#define OMRX_DTYPE_S64        (OMRX_TYPEF_SIMPLE | OMRX_TYPEF_SIGNED   | 3)
#define OMRX_DTYPE_F64        (OMRX_TYPEF_SIMPLE | OMRX_TYPEF_FLOAT    | 3)
#define OMRX_DTYPE_U8_ARRAY   (OMRX_TYPEF_ARRAY  | OMRX_DTYPE_U8)
#define OMRX_DTYPE_S8_ARRAY   (OMRX_TYPEF_ARRAY  | OMRX_DTYPE_S8)
#define OMRX_DTYPE_U16_ARRAY  (OMRX_TYPEF_ARRAY  | OMRX_DTYPE_U16)
#define OMRX_DTYPE_S16_ARRAY  (OMRX_TYPEF_ARRAY  | OMRX_DTYPE_S16)
#define OMRX_DTYPE_U32_ARRAY  (OMRX_TYPEF_ARRAY  | OMRX_DTYPE_U32)
#define OMRX_DTYPE_S32_ARRAY  (OMRX_TYPEF_ARRAY  | OMRX_DTYPE_S32)
#define OMRX_DTYPE_F32_ARRAY  (OMRX_TYPEF_ARRAY  | OMRX_DTYPE_F32)
#define OMRX_DTYPE_U64_ARRAY  (OMRX_TYPEF_ARRAY  | OMRX_DTYPE_U64)
#define OMRX_DTYPE_S64_ARRAY  (OMRX_TYPEF_ARRAY  | OMRX_DTYPE_S64)
#define OMRX_DTYPE_F64_ARRAY  (OMRX_TYPEF_ARRAY  | OMRX_DTYPE_F64)
#define OMRX_DTYPE_UTF8       (OMRX_TYPEF_OTHER  | 0x000)
#define OMRX_DTYPE_RAW        (OMRX_TYPEF_OTHER  | 0x001)

#define OMRX_GET_SUBTYPE(dtype) ((dtype) & 0xff00)
#define OMRX_GET_ELEMTYPE(dtype) ((dtype) & 0x00ff)
// Note: The following only works for SIMPLE and ARRAY types:
#define OMRX_GET_ELEMSIZE(dtype) (1 << ((dtype) & 0x0003))

#define OMRX_IS_ARRAY_DTYPE(dtype) (OMRX_GET_SUBTYPE(dtype) == OMRX_TYPEF_ARRAY)
#define OMRX_IS_SIMPLE_DTYPE(dtype) (OMRX_GET_SUBTYPE(dtype) == OMRX_TYPEF_SIMPLE)
#define OMRX_IS_OTHER_DTYPE(dtype) (OMRX_GET_SUBTYPE(dtype) == OMRX_TYPEF_OTHER)

#define OMRX_ATTR_VER  0x0000
#define OMRX_ATTR_ID   0x0001
#define OMRX_ATTR_DATA 0xffff

#define OMRX_VERSION 0x00000001
#define OMRX_MIN_VERSION 0x00000001

#define OMRX_VER_MAJOR(x) ((x) >> 16)
#define OMRX_VER_MINOR(x) ((x) & 0xffff)

#define OMRX_API_VER 0

struct omrx_attr_info {
    bool exists;
    uint16_t encoded_type;
    uint32_t size;
    uint16_t elem_type;
    uint32_t elem_size;
    bool is_array;
    uint16_t cols;
    uint32_t rows;
};

void omrx_default_log_warning_func(omrx_t omrx, omrx_status_t errcode, const char *msg);
void omrx_default_log_error_func(omrx_t omrx, omrx_status_t errcode, const char *msg);

omrx_status_t omrx_do_init(int api_ver, omrx_log_func_t warnfunc, omrx_log_func_t errfunc);
omrx_status_t omrx_new(omrx_t *result);
omrx_status_t omrx_free(omrx_t omrx);
omrx_status_t omrx_status(omrx_t omrx, bool reset);
omrx_status_t omrx_last_result(omrx_t omrx);
omrx_status_t omrx_get_version(omrx_t omrx, uint32_t *ver);
omrx_status_t omrx_open(omrx_t omrx, const char *filename, FILE *fp);
omrx_status_t omrx_close(omrx_t omrx);
omrx_status_t omrx_get_root_chunk(omrx_t omrx, omrx_chunk_t *result);
omrx_status_t omrx_get_child(omrx_chunk_t chunk, const char *tag, omrx_chunk_t *result);
omrx_status_t omrx_get_next_chunk(omrx_chunk_t chunk, const char *tag, omrx_chunk_t *result);
omrx_status_t omrx_get_chunk_by_id(omrx_t omrx, const char *id, const char *tag, omrx_chunk_t *result);
omrx_status_t omrx_get_child_by_id(omrx_chunk_t chunk, const char *tag, const char *id, omrx_chunk_t *result);
omrx_status_t omrx_get_parent(omrx_chunk_t chunk, omrx_chunk_t *result);
omrx_status_t omrx_add_chunk(omrx_chunk_t chunk, const char *tag, omrx_chunk_t *result);
omrx_status_t omrx_del_chunk(omrx_chunk_t chunk);
omrx_status_t omrx_get_attr_info(omrx_chunk_t chunk, uint16_t id, struct omrx_attr_info *info);
omrx_status_t omrx_set_attr_str(omrx_chunk_t chunk, uint16_t id, omrx_ownership_t own, char *str);
omrx_status_t omrx_get_attr_str(omrx_chunk_t chunk, uint16_t id, char **dest);
omrx_status_t omrx_set_attr_uint32(omrx_chunk_t chunk, uint16_t id, uint32_t value);
omrx_status_t omrx_get_attr_uint32(omrx_chunk_t chunk, uint16_t id, uint32_t *dest);
omrx_status_t omrx_set_attr_float32_array(omrx_chunk_t chunk, uint16_t id, omrx_ownership_t own, uint16_t cols, uint32_t rows, float *data);
omrx_status_t omrx_get_attr_float32_array(omrx_chunk_t chunk, uint16_t id, uint16_t *cols, uint32_t *rows, float **data);
omrx_status_t omrx_release_attr_data(omrx_chunk_t chunk, uint16_t id);
omrx_status_t omrx_del_attr(omrx_chunk_t chunk, uint16_t id);
omrx_status_t omrx_write(omrx_t omrx, const char *filename);

#define omrx_init() omrx_do_init(OMRX_API_VER, omrx_default_log_warning_func, omrx_default_log_error_func)

#ifdef  __cplusplus
}
#endif

#endif /* _OMRX_H */
