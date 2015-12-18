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

typedef void (omrx_log_func_t)(omrx_t omrx, omrx_status_t errcode, const char *msg);

#define OMRX_OK 0
#define OMRX_NONE 1

#define OMRX_WARN_BADATTR 0x100

#define OMRX_ERR_OSERR        -1
#define OMRX_ERR_EOF          -2
#define OMRX_ERR_NOT_OPEN     -3
#define OMRX_ERR_ALREADY_OPEN -4
#define OMRX_ERR_BAD_MAGIC    -5
#define OMRX_ERR_BAD_VER      -6
#define OMRX_ERR_BAD_CHUNK    -7
#define OMRX_ERR_BAD_DTYPE    -8
#define OMRX_ERR_NODATA       -9

#define OMRX_STYPE_SIMPLE 0x0000
#define OMRX_STYPE_ARRAY  0x1000
#define OMRX_STYPE_OTHER  0xf000

#define OMRX_DTYPE_U32        (OMRX_STYPE_SIMPLE | 0x000)
#define OMRX_DTYPE_S32        (OMRX_STYPE_SIMPLE | 0x001)
#define OMRX_DTYPE_F32        (OMRX_STYPE_SIMPLE | 0x002)
#define OMRX_DTYPE_F32_ARRAY  (OMRX_STYPE_ARRAY  | 0x002)
#define OMRX_DTYPE_UTF8       (OMRX_STYPE_SIMPLE | 0x003)
#define OMRX_DTYPE_RAW        (OMRX_STYPE_OTHER  | 0x000)

#define OMRX_GET_SUBTYPE(dtype) ((dtype) & 0xff00)
#define OMRX_IS_ARRAY_DTYPE(dtype) (OMRX_GET_SUBTYPE(dtype) == OMRX_STYPE_ARRAY)

#define OMRX_ATTR_VER  0x0000
#define OMRX_ATTR_ID   0x0001
#define OMRX_ATTR_DATA 0xffff

#define OMRX_MINVER_MAJOR 0
#define OMRX_MINVER_MINOR 1

#define OMRX_SUPVER_MAJOR 0
#define OMRX_SUPVER_MINOR 1

#define OMRX_API_VER 0

bool do_omrx_init(int api_ver);
omrx_t omrx_new(void);
omrx_status_t omrx_free(omrx_t omrx);
omrx_status_t omrx_open(omrx_t omrx, const char *filename);
omrx_status_t omrx_close(omrx_t omrx);
omrx_chunk_t omrx_get_root_chunk(omrx_t omrx);
omrx_status_t omrx_get_first_chunk(omrx_t omrx, const char *tag, omrx_chunk_t *chunk);
omrx_status_t omrx_get_next_chunk(omrx_chunk_t chunk, omrx_chunk_t *next_chunk);
omrx_status_t omrx_get_chunk_by_id(omrx_t omrx, const char *tag, const char *id, omrx_chunk_t *chunk);
omrx_status_t omrx_add_chunk(omrx_chunk_t parent, const char *tag, omrx_chunk_t *new_chunkptr);
omrx_status_t omrx_del_chunk(omrx_chunk_t chunk);
omrx_status_t omrx_set_attr_str(omrx_chunk_t chunk, uint16_t id, omrx_ownership_t own, char *str);
omrx_status_t omrx_get_attr_str(omrx_chunk_t chunk, uint16_t id, char **dest);
omrx_status_t omrx_set_attr_float_array(omrx_chunk_t chunk, uint16_t id, omrx_ownership_t own, uint16_t cols, uint32_t rows, float *data);
omrx_status_t omrx_release_attr_data(omrx_chunk_t chunk, uint16_t id);
omrx_status_t omrx_del_attr(omrx_chunk_t chunk, uint16_t id);
omrx_status_t omrx_write(omrx_t omrx, const char *filename);

#define omrx_init() do_omrx_init(OMRX_API_VER)

#ifdef  __cplusplus
}
#endif

#endif /* _OMRX_H */
