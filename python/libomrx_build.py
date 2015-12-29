#!/usr/bin/python

from cffi import FFI
ffi = FFI()

ffi.set_source("_libomrx_cffi", """
    #include <omrx.h>

    static void _log_warning(omrx_t omrx, omrx_status_t errcode, const char *msg);
    static void _log_error(omrx_t omrx, omrx_status_t errcode, const char *msg);

    // Make our own version of omrx_init() that's an actual function (so it can
    // be called from Python) and invokes our Python-callback logging
    // functions instead of the default ones.
    #undef omrx_init
    omrx_status_t omrx_init(void) {
        return omrx_initialize(OMRX_API_VER, _log_warning, _log_error, NULL, NULL);
    }

""", libraries=['omrx'], library_dirs=['../lib'], include_dirs=['../include'])

ffi.cdef("""
    typedef enum { OMRX_TAKE, OMRX_COPY, ...} omrx_ownership_t;
    typedef struct omrx *omrx_t;
    typedef struct omrx_chunk *omrx_chunk_t;

    #define OMRX_WARNING ...

    typedef enum { OMRX_OK, OMRX_STATUS_OK, OMRX_STATUS_NOT_FOUND, OMRX_STATUS_DUP, OMRX_STATUS_NO_OBJECT, OMRX_WARN_BAD_VER, OMRX_WARN_BAD_ATTR, OMRX_WARN_OSERR, OMRX_ERR_BADAPI, OMRX_ERR_INIT_FIRST, OMRX_ERR_OSERR, OMRX_ERR_ALLOC, OMRX_ERR_EOF, OMRX_ERR_NOT_OPEN, OMRX_ERR_ALREADY_OPEN, OMRX_ERR_BAD_MAGIC, OMRX_ERR_BAD_VER, OMRX_ERR_BAD_CHUNK, OMRX_ERR_WRONG_DTYPE, OMRX_ERR_INTERNAL, ...} omrx_status_t;

    typedef enum { OMRX_DTYPE_U8, OMRX_DTYPE_S8, OMRX_DTYPE_U16, OMRX_DTYPE_S16, OMRX_DTYPE_U32, OMRX_DTYPE_S32, OMRX_DTYPE_F32, OMRX_DTYPE_U64, OMRX_DTYPE_S64, OMRX_DTYPE_F64, OMRX_DTYPE_U8_ARRAY, OMRX_DTYPE_S8_ARRAY, OMRX_DTYPE_U16_ARRAY, OMRX_DTYPE_S16_ARRAY, OMRX_DTYPE_U32_ARRAY, OMRX_DTYPE_S32_ARRAY, OMRX_DTYPE_F32_ARRAY, OMRX_DTYPE_U64_ARRAY, OMRX_DTYPE_S64_ARRAY, OMRX_DTYPE_F64_ARRAY, OMRX_DTYPE_UTF8, OMRX_DTYPE_RAW, ...} omrx_dtype_t;

    #define OMRX_ATTR_VER  ...
    #define OMRX_ATTR_ID   ...
    #define OMRX_ATTR_DATA ...

    #define OMRX_VERSION     ...
    #define OMRX_MIN_VERSION ...

    #define OMRX_API_VER ...

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

    // Callback functions into Python
    extern "Python" void _log_warning(omrx_t omrx, omrx_status_t errcode, const char *msg);
    extern "Python" void _log_error(omrx_t omrx, omrx_status_t errcode, const char *msg);

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
    omrx_status_t omrx_add_chunk(omrx_chunk_t chunk, const char *tag, omrx_chunk_t *result);
    omrx_status_t omrx_del_chunk(omrx_chunk_t chunk);
    omrx_status_t omrx_get_attr_info(omrx_chunk_t chunk, uint16_t id, struct omrx_attr_info *info);
    omrx_status_t omrx_get_attr_raw(omrx_chunk_t chunk, uint16_t id, size_t *size, void **data);
    omrx_status_t omrx_set_attr_str(omrx_chunk_t chunk, uint16_t id, omrx_ownership_t own, char *str);
    omrx_status_t omrx_get_attr_str(omrx_chunk_t chunk, uint16_t id, char **dest);
    omrx_status_t omrx_set_attr_uint32(omrx_chunk_t chunk, uint16_t id, uint32_t value);
    omrx_status_t omrx_get_attr_uint32(omrx_chunk_t chunk, uint16_t id, uint32_t *dest);
    omrx_status_t omrx_set_attr_float32_array(omrx_chunk_t chunk, uint16_t id, omrx_ownership_t own, uint16_t cols, uint32_t rows, float *data);
    omrx_status_t omrx_get_attr_float32_array(omrx_chunk_t chunk, uint16_t id, uint16_t *cols, uint32_t *rows, float **data);
    omrx_status_t omrx_release_attr_data(omrx_chunk_t chunk, uint16_t id);
    omrx_status_t omrx_del_attr(omrx_chunk_t chunk, uint16_t id);
    omrx_status_t omrx_write(omrx_t omrx, const char *filename);

    omrx_status_t omrx_init(void);
""")

if __name__ == "__main__":
    ffi.compile()
