#ifndef _OMRX_H
#define _OMRX_H

#include <stdint.h>
#include <stdbool.h>

/** @file */

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum {
    //TODO: Implement OMRX_REF
    OMRX_TAKE,
    OMRX_COPY,
} omrx_ownership_t;

typedef enum {
    OMRX_MESH_VERTICES,
    OMRX_MESH_NORMALS,
    OMRX_MESH_MAT_IDX,
    OMRX_MESH_TEXCOORDS,
} omrx_meshdata_type_t;

typedef enum {
    OMRX_POLY_TRISTRIPS,
    OMRX_POLY_TRIANGLES,
    OMRX_POLY_QUADS,
    OMRX_POLY_INVALID,
} omrx_poly_type_t;

/** @brief Opaque handle to an OMRX instance.
  *
  * Each OMRX instance represents a separate OMRX file.
  *
  * @ingroup api
  */
typedef struct omrx *omrx_t;

/** @brief Opaque handle to an OMRX chunk.
  *
  * Used in the libomrx Chunk-level API.
  *
  * Each OMRX instance has one or more chunks, and each chunk can contain zero
  * or more data attributes.
  *
  * @ingroup api
  */
typedef struct omrx_chunk *omrx_chunk_t;

#define OMRX_WARNING        0x1000

/** @brief Status codes returned by (almost) all libomrx API functions
  *
  * @ingroup api
  */

typedef enum {
    /** This is an alias for ::OMRX_STATUS_OK */
    OMRX_OK               = 0,
    /** Operation completed successfully */
    OMRX_STATUS_OK        = 0,
    /** The specified resource was not found */
    OMRX_STATUS_NOT_FOUND = 1,
    /** Entry already exists */
    OMRX_STATUS_DUP       = 2,
    /** NULL was passed instead of a valid parent object */
    OMRX_STATUS_NO_OBJECT = 3,

    /** The opened OMRX file was written to a version of the OMRX specification newer than what this library supports.  The version is backwards-compatible, so some portions of the file may still be readable as expected, but other portions or intended functionality may not be available when using this library version. (The version of the file can be obtained with omrx_get_version()) */
    OMRX_WARN_BAD_VER     = OMRX_WARNING + 0,

     /** One or more attributes encountered have the wrong type or bad data and could not be read.  This did not cause the current operation to fail, but other operations may not produce the expected results because the data could not be read (for example, if an `id` attribute is bad, future calls to omrx_get_chunk_by_id() may fail to return the desired chunk) */
    OMRX_WARN_BAD_ATTR    = OMRX_WARNING + 1,

    /** A call to an OS function resulted in an error.  This error did not prevent the current operation from completing, but may indicate that some portions of it did not work as expected (for example, a call to omrx_close() may not have been able to actually close the underlying file handle). */
    OMRX_WARN_OSERR       = OMRX_WARNING + 2,

    /** The API version constant passed to omrx_initialize() does not match the version the library was compiled with.  (This generally indicates the application was compiled with a different version of the headers than the library was) */
    OMRX_ERR_BADAPI       = -1,

    /** Attempt to call other libomrx functions before omrx_initialize() */
    OMRX_ERR_INIT_FIRST   = -2,

    /** A call to an OS function resulted in an error and the operation could not continue. */
    OMRX_ERR_OSERR        = -3,

    /** An attempt to allocate memory failed. */
    OMRX_ERR_ALLOC        = -4,

    /** An unexpected end-of-file was encountered when reading the input file. */
    OMRX_ERR_EOF          = -5,

    /** omrx_close() was called on an OMRX instance which was not previously opened with omrx_open() */
    OMRX_ERR_NOT_OPEN     = -6,

    /** omrx_open() was called on an OMRX instance that was already open */
    OMRX_ERR_ALREADY_OPEN = -7,

    /** The opened file does not appear to be an OMRX file */
    OMRX_ERR_BAD_MAGIC    = -8,

    /** The opened OMRX file was written to a version of the OMRX specification newer than what this library supports.  The version is NOT backwards-compatible, and thus cannot be read at all by this version of the library. */
    OMRX_ERR_BAD_VER      = -9,

    /** Bad or corrupted data was encountered when reading the file (invalid chunk tag encountered) */
    OMRX_ERR_BAD_CHUNK    = -10,

    /** An attempt was made to fetch data from an attribute which is incompatible with the type of data actually contained in the attribute */
    OMRX_ERR_WRONG_DTYPE  = -11,
    
    /** An attempt was made to use a chunk in a way that does not match its type */
    OMRX_ERR_WRONG_CHUNK  = -12,
    
    /** An attempt was made to reference a chunk by index, but that index is out of range or otherwise invalid */
    OMRX_ERR_BAD_INDEX    = -13,

    OMRX_ERR_BAD_SIZE     = -14,

    /** Internal error (this indicates a bug somewhere inside libomrx) */
    OMRX_ERR_INTERNAL     = -500,
} omrx_status_t;


#define OMRX_TYPEF_UNSIGNED 0x0000
#define OMRX_TYPEF_SIGNED   0x0010
#define OMRX_TYPEF_FLOAT    0x0020
#define OMRX_TYPEF_SIMPLE   0x0000
#define OMRX_TYPEF_ARRAY    0x1000
#define OMRX_TYPEF_OTHER    0xf000

typedef enum {
    OMRX_DTYPE_U8        = OMRX_TYPEF_SIMPLE | OMRX_TYPEF_UNSIGNED | 0,
    OMRX_DTYPE_S8        = OMRX_TYPEF_SIMPLE | OMRX_TYPEF_SIGNED   | 0,
    OMRX_DTYPE_U16       = OMRX_TYPEF_SIMPLE | OMRX_TYPEF_UNSIGNED | 1,
    OMRX_DTYPE_S16       = OMRX_TYPEF_SIMPLE | OMRX_TYPEF_SIGNED   | 1,
    OMRX_DTYPE_U32       = OMRX_TYPEF_SIMPLE | OMRX_TYPEF_UNSIGNED | 2,
    OMRX_DTYPE_S32       = OMRX_TYPEF_SIMPLE | OMRX_TYPEF_SIGNED   | 2,
    OMRX_DTYPE_F32       = OMRX_TYPEF_SIMPLE | OMRX_TYPEF_FLOAT    | 2,
    OMRX_DTYPE_U64       = OMRX_TYPEF_SIMPLE | OMRX_TYPEF_UNSIGNED | 3,
    OMRX_DTYPE_S64       = OMRX_TYPEF_SIMPLE | OMRX_TYPEF_SIGNED   | 3,
    OMRX_DTYPE_F64       = OMRX_TYPEF_SIMPLE | OMRX_TYPEF_FLOAT    | 3,
    OMRX_DTYPE_U8_ARRAY  = OMRX_TYPEF_ARRAY  | OMRX_DTYPE_U8,
    OMRX_DTYPE_S8_ARRAY  = OMRX_TYPEF_ARRAY  | OMRX_DTYPE_S8,
    OMRX_DTYPE_U16_ARRAY = OMRX_TYPEF_ARRAY  | OMRX_DTYPE_U16,
    OMRX_DTYPE_S16_ARRAY = OMRX_TYPEF_ARRAY  | OMRX_DTYPE_S16,
    OMRX_DTYPE_U32_ARRAY = OMRX_TYPEF_ARRAY  | OMRX_DTYPE_U32,
    OMRX_DTYPE_S32_ARRAY = OMRX_TYPEF_ARRAY  | OMRX_DTYPE_S32,
    OMRX_DTYPE_F32_ARRAY = OMRX_TYPEF_ARRAY  | OMRX_DTYPE_F32,
    OMRX_DTYPE_U64_ARRAY = OMRX_TYPEF_ARRAY  | OMRX_DTYPE_U64,
    OMRX_DTYPE_S64_ARRAY = OMRX_TYPEF_ARRAY  | OMRX_DTYPE_S64,
    OMRX_DTYPE_F64_ARRAY = OMRX_TYPEF_ARRAY  | OMRX_DTYPE_F64,
    OMRX_DTYPE_UTF8      = OMRX_TYPEF_OTHER  | 0x000,
    OMRX_DTYPE_RAW       = OMRX_TYPEF_OTHER  | 0x001,
    OMRX_DTYPE_INVALID   = OMRX_TYPEF_OTHER  | 0xfff,
} omrx_dtype_t;

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

/** @brief An integer indicating the version of the libomrx API in use.
  *
  * This API version must match between the application and the library, and
  * must be passed as the first argument to omrx_initialize()
  *
  * @ingroup api
  */
#define OMRX_API_VER 0

typedef void (*omrx_log_func_t)(omrx_t omrx, omrx_status_t errcode, const char *msg);
typedef void *(*omrx_alloc_func_t)(omrx_t omrx, size_t size);
typedef void (*omrx_free_func_t)(omrx_t omrx, void *ptr);

struct omrx_attr_info {
    bool exists;
    uint16_t encoded_type;
    uint16_t raw_type;
    uint32_t size;
    bool is_array;
    uint16_t elem_type;
    uint32_t elem_size;
    uint16_t cols;
    uint32_t rows;
};

void omrx_default_log_warning(omrx_t omrx, omrx_status_t errcode, const char *msg);
void omrx_default_log_error(omrx_t omrx, omrx_status_t errcode, const char *msg);

omrx_status_t omrx_initialize(int api_ver, omrx_log_func_t warn_func, omrx_log_func_t err_func, omrx_alloc_func_t alloc_func, omrx_free_func_t free_func);
omrx_status_t omrx_new(void *user_data, omrx_t *result);
omrx_status_t omrx_free(omrx_t omrx);
void *omrx_user_data(omrx_t omrx);
omrx_status_t omrx_status(omrx_t omrx, bool reset);
omrx_status_t omrx_last_result(omrx_t omrx);
omrx_status_t omrx_get_version(omrx_t omrx, uint32_t *result);
omrx_status_t omrx_open(omrx_t omrx, const char *filename, FILE *fp);
omrx_status_t omrx_close(omrx_t omrx);
omrx_status_t omrx_get_root_chunk(omrx_t omrx, omrx_chunk_t *result);
omrx_status_t omrx_get_child(omrx_chunk_t chunk, const char *tag, omrx_chunk_t *result);
omrx_status_t omrx_get_next_chunk(omrx_chunk_t chunk, const char *tag, omrx_chunk_t *result);
omrx_status_t omrx_get_chunk_by_id(omrx_t omrx, const char *id, const char *tag, omrx_chunk_t *result);
omrx_status_t omrx_get_child_by_id(omrx_chunk_t chunk, const char *tag, const char *id, omrx_chunk_t *result);
omrx_status_t omrx_get_parent(omrx_chunk_t chunk, omrx_chunk_t *result);
omrx_status_t omrx_add_chunk(omrx_chunk_t chunk, int index, const char *tag, omrx_chunk_t *result);
omrx_status_t omrx_del_chunk(omrx_chunk_t chunk);
omrx_status_t omrx_get_attr_info(omrx_chunk_t chunk, uint16_t id, struct omrx_attr_info *info);
omrx_status_t omrx_get_attr_raw(omrx_chunk_t chunk, uint16_t id, size_t *size, void **data);
omrx_status_t omrx_set_attr_str(omrx_chunk_t chunk, uint16_t id, omrx_ownership_t own, char *str);
omrx_status_t omrx_get_attr_str(omrx_chunk_t chunk, uint16_t id, char **dest);
omrx_status_t omrx_set_attr_uint32(omrx_chunk_t chunk, uint16_t id, uint32_t value);
omrx_status_t omrx_get_attr_uint32(omrx_chunk_t chunk, uint16_t id, uint32_t *dest);
omrx_status_t omrx_set_attr_float32_array(omrx_chunk_t chunk, uint16_t id, omrx_ownership_t own, uint16_t cols, uint32_t rows, float *data);
omrx_status_t omrx_get_attr_float32_array(omrx_chunk_t chunk, uint16_t id, uint16_t *cols, uint32_t *rows, float **data);
omrx_status_t omrx_release_attr_data(omrx_chunk_t chunk, uint16_t id, bool free_memory);
omrx_status_t omrx_del_attr(omrx_chunk_t chunk, uint16_t id);
omrx_status_t omrx_write(omrx_t omrx, const char *filename);

#define omrx_init() omrx_initialize(OMRX_API_VER, omrx_default_log_warning, omrx_default_log_error, NULL, NULL)

// Model API

struct omrx_meshdata {
    void *data;
    omrx_dtype_t datatype;
    uint_fast16_t cols;
    uint_fast32_t rows;
};

struct omrx_polys {
    omrx_poly_type_t polytype;
    void *data;
    omrx_dtype_t datatype;
    uint_fast32_t count;
};

#ifdef  __cplusplus
}
#endif

#endif /* _OMRX_H */
