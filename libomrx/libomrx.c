#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "omrx.h"

//TODO: make sure zero-length arrays are handled properly (malloc, read/write, etc)

#define LOG(...) fprintf(stderr, __VA_ARGS__); fflush(stderr);

#ifdef LOGIO
  #define LOG_IO(...) LOG(__VA_ARGS__)
#else
  #define LOG_IO(...) /* do nothing */
#endif

#define OMRX_ERRMSG_BUFSIZE 4096

typedef struct omrx_attr *omrx_attr_t;

struct omrx {
    FILE *fp;
    char *filename;
    char *message;
    omrx_log_func_t *log_error;
    omrx_log_func_t *log_warning;
    struct omrx_chunk *top_chunk;
    struct omrx_chunk *context;
    omrx_status_t status;
    omrx_status_t last_result;
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

#define CHECK_ALLOC(omrx, x) if ((x) == NULL) { return omrx_os_error((omrx), "Memory allocation failed"); }
#define CHECK_ERR(x) do { omrx_status_t __x = (x); if (__x < 0) return __x; } while (0);
#define RESULT(omrx, x) (omrx->last_result = (x))

#define CHUNKHDR_SIZE 6
#define ATTRHDR_SIZE 8

struct chunk_header {
    char tag[4];
    uint16_t count;
};
_Static_assert(sizeof(struct chunk_header) == CHUNKHDR_SIZE, "struct chunk_header is the wrong size");

struct attr_header {
    uint16_t id;
    uint16_t datatype;
    uint32_t size;
};
_Static_assert(sizeof(struct attr_header) == ATTRHDR_SIZE, "struct attr_header is the wrong size");

static void omrx_default_log_error_func(omrx_t omrx, omrx_status_t errcode, const char *msg);
static void omrx_default_log_warning_func(omrx_t omrx, omrx_status_t errcode, const char *msg);
static omrx_status_t seek_to_pos(omrx_t omrx, off_t pos);
static omrx_status_t skip_data(omrx_t omrx, off_t size);
static omrx_status_t read_data(omrx_t omrx, off_t size, void *dest);
static omrx_status_t write_data(omrx_t omrx, off_t size, const void *src, FILE *fp);

static omrx_chunk_t new_chunk(omrx_t omrx, const char *tag);
static omrx_status_t free_chunk(omrx_chunk_t chunk);
static omrx_status_t free_all_chunks(omrx_chunk_t chunk);
static omrx_attr_t new_attr(omrx_chunk_t chunk, uint16_t id, uint16_t datatype, uint32_t size, off_t file_pos);
static omrx_status_t free_attr(omrx_attr_t attr);

static omrx_status_t load_attr_data(omrx_attr_t attr);
static omrx_status_t release_attr_data(omrx_attr_t attr);
static omrx_status_t freeze_attr_data(omrx_attr_t attr);
static omrx_status_t find_attr(omrx_chunk_t chunk, uint16_t id, omrx_attr_t *dest);
static omrx_status_t chunk_add_attr(omrx_chunk_t chunk, omrx_attr_t attr);
static omrx_status_t add_child_chunk(omrx_chunk_t parent, omrx_chunk_t child);
static omrx_status_t omrx_scan(omrx_t omrx);
static omrx_status_t read_next_chunk(omrx_t omrx);
static omrx_status_t read_attr_subheader_array(omrx_attr_t attr);
static omrx_status_t write_chunk(omrx_chunk_t chunk, FILE *fp);
static omrx_status_t write_attr_subheader_array(omrx_attr_t attr, FILE *fp);
static omrx_status_t write_attr(omrx_attr_t attr, FILE *fp);
static uint32_t get_elem_size(uint16_t dtype, uint32_t total_size);

///////////////////////////////////////////////

static void omrx_default_log_error_func(omrx_t omrx, omrx_status_t errcode, const char *msg) {
    if (omrx->filename) {
        fprintf(stderr, "libomrx error: %s: %s\n", omrx->filename, msg);
    } else {
        fprintf(stderr, "libomrx error: %s\n", msg);
    }
    fflush(stderr);
}

static void omrx_default_log_warning_func(omrx_t omrx, omrx_status_t errcode, const char *msg) {
    if (omrx->filename) {
        fprintf(stderr, "libomrx warning: %s: %s\n", omrx->filename, msg);
    } else {
        fprintf(stderr, "libomrx warning: %s\n", msg);
    }
    fflush(stderr);
}

static omrx_log_func_t *default_log_error = omrx_default_log_error_func;
static omrx_log_func_t *default_log_warning = omrx_default_log_warning_func;

omrx_status_t omrx_error(omrx_t omrx, omrx_status_t errcode, const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    if (vsnprintf(omrx->message, OMRX_ERRMSG_BUFSIZE, fmt, ap) < 0) {
        strncpy(omrx->message, "(unable to format error message)", OMRX_ERRMSG_BUFSIZE);
        omrx->message[OMRX_ERRMSG_BUFSIZE - 1] = 0;
    }
    va_end(ap);
    if (omrx->log_error) {
        omrx->log_error(omrx, errcode, omrx->message);
    }

    omrx->status = errcode;
    omrx->last_result = errcode;
    return errcode;
}

omrx_status_t omrx_os_error(omrx_t omrx, const char *fmt, ...) {
    va_list ap;
    size_t msglen;
    omrx_status_t errcode;

    if (errno == 0 && omrx->fp && feof(omrx->fp)) {
        // We must have gotten here because of a short read due to hitting
        // EOF unexpectedly.
        errcode = OMRX_ERR_EOF;
    } else {
        errcode = OMRX_ERR_OSERR;
    }

    va_start(ap, fmt);
    if (vsnprintf(omrx->message, OMRX_ERRMSG_BUFSIZE, fmt, ap) < 0) {
        strncpy(omrx->message, "(unable to format error message)", OMRX_ERRMSG_BUFSIZE);
        omrx->message[OMRX_ERRMSG_BUFSIZE - 1] = 0;
    }
    va_end(ap);

    msglen = strlen(omrx->message);
    // If there's space, tack on the strerror() message after our own message.
    if (msglen < OMRX_ERRMSG_BUFSIZE - 3) {
        omrx->message[msglen] = ':';
        omrx->message[msglen + 1] = ' ';
        if (errcode == OMRX_ERR_EOF) {
            strncpy(omrx->message + msglen + 2, "Unexpected end of file", OMRX_ERRMSG_BUFSIZE - msglen - 2);
            omrx->message[OMRX_ERRMSG_BUFSIZE - 1] = 0;
        } else {
            if (strerror_r(errno, omrx->message + msglen + 2, OMRX_ERRMSG_BUFSIZE - msglen - 2)) {
                omrx->message[msglen + 2] = 0; // FIXME
            }
        }
    }

    if (omrx->log_error) {
        omrx->log_error(omrx, errcode, omrx->message);
    }

    omrx->status = errcode;
    omrx->last_result = errcode;
    return errcode;
}

omrx_status_t omrx_warning(omrx_t omrx, omrx_status_t errcode, const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    if (vsnprintf(omrx->message, OMRX_ERRMSG_BUFSIZE, fmt, ap) < 0) {
        strncpy(omrx->message, "(unable to format error message)", OMRX_ERRMSG_BUFSIZE);
        omrx->message[OMRX_ERRMSG_BUFSIZE - 1] = 0;
    }
    va_end(ap);
    if (omrx->log_warning) {
        omrx->log_warning(omrx, errcode, omrx->message);
    }

    // Any previous error status takes priority, but if the current status is a
    // warning or less, replace it with the most recent warning status.
    if (omrx->status >= 0) {
        omrx->status = errcode;
    }
    omrx->last_result = errcode;
    return errcode;
}

///////////////////////////////////

static omrx_status_t seek_to_pos(omrx_t omrx, off_t pos) {
    LOG_IO("- seek %lu\n", pos);
    if (fseeko(omrx->fp, pos, SEEK_SET) < 0) {
        return omrx_os_error(omrx, "Seek failed");
    }

    return OMRX_OK;
}

static omrx_status_t skip_data(omrx_t omrx, off_t size) {
    LOG_IO("- skip %lu\n", size);
    if (fseeko(omrx->fp, size, SEEK_CUR) < 0) {
        return omrx_os_error(omrx, "Seek failed");
    }

    return OMRX_OK;
}

static omrx_status_t read_data(omrx_t omrx, off_t size, void *dest) {
    if (fread(dest, size, 1, omrx->fp) != 1) {
        return omrx_os_error(omrx, "Read error");
    }
#if LOGIO
    LOG_IO("- read: ");
    int i;
    for (i = 0; i < size; i++) {
        LOG_IO("%02x ", ((uint8_t *)dest)[i]);
    }
    LOG_IO("\n");
#endif

    return OMRX_OK;
}

static omrx_status_t write_data(omrx_t omrx, off_t size, const void *src, FILE *fp) {
    if (!size) return OMRX_OK;

#if LOGIO
    LOG_IO("- write: ");
    int i;
    for (i = 0; i < size; i++) {
        LOG_IO("%02x ", ((uint8_t *)src)[i]);
    }
    LOG_IO("\n");
#endif

    if (fwrite(src, size, 1, fp) != 1) {
        return omrx_os_error(omrx, "Write error");
    }

    return OMRX_OK;
}

///////////////////////////////////

static omrx_chunk_t new_chunk(omrx_t omrx, const char *tag) {
    omrx_chunk_t chunk;

    chunk = malloc(sizeof(struct omrx_chunk));
    if (!chunk) return NULL;

    chunk->omrx = omrx;
    chunk->next = NULL;
    chunk->parent = NULL;
    chunk->first_child = NULL;
    chunk->last_child = NULL;
    chunk->id = NULL;
    chunk->tag[0] = tag[0];
    chunk->tag[1] = tag[1];
    chunk->tag[2] = tag[2];
    chunk->tag[3] = tag[3];
    chunk->tag[4] = 0;
    chunk->tagint = TAG_TO_TAGINT(tag);
    chunk->attr_count = 0;
    chunk->attrs = NULL;
    return chunk;
}

static omrx_status_t free_chunk(omrx_chunk_t chunk) {
    omrx_attr_t attr = chunk->attrs;
    omrx_attr_t next_attr;
    omrx_status_t status = OMRX_OK;
    omrx_status_t rc;

    while (attr) {
        next_attr = attr->next;
        rc = free_attr(attr);
        if (rc != OMRX_OK) status = rc;
        attr = next_attr;
    }
    free(chunk);

    return status;
}

static omrx_status_t free_all_chunks(omrx_chunk_t chunk) {
    omrx_status_t status = OMRX_OK;
    omrx_status_t rc;
    omrx_chunk_t next_chunk;

    // FIXME: make this non-recursive
    while (chunk) {
        if (chunk->first_child) {
            rc = free_all_chunks(chunk->first_child);
            if (rc != OMRX_OK) status = rc;
        }
        next_chunk = chunk->next;
        rc = free_chunk(chunk);
        if (rc != OMRX_OK) status = rc;
        chunk = next_chunk;
    }

    return status;
}

static omrx_attr_t new_attr(omrx_chunk_t chunk, uint16_t id, uint16_t datatype, uint32_t size, off_t file_pos) {
    omrx_attr_t attr;

    attr = malloc(sizeof(struct omrx_attr));
    if (!attr) return NULL;

    attr->chunk = chunk;
    attr->next = NULL;
    attr->id = id;
    attr->datatype = datatype;
    attr->size = size;
    attr->file_pos = file_pos;
    attr->data = NULL;
    attr->cols = 1;

    return attr;
}

static omrx_status_t free_attr(omrx_attr_t attr) {
    if (attr->data) {
        // FIXME: need to check if anybody's using it still
        free(attr->data);
    }
    free(attr);

    return OMRX_OK;
}

static omrx_status_t load_attr_data(omrx_attr_t attr) {
    omrx_t omrx = attr->chunk->omrx;

    if (attr->data) {
        // Already loaded (or a non-file-backed attr with data already defined)
        return OMRX_OK;
    }
    if (attr->file_pos < 0) {
        // Somebody called load_attr_data on a non-file-backed attribute.
        // This is probably because somebody created a new (not read from a
        // file) attribute and forgot to assign data to it.
        return omrx_error(omrx, OMRX_ERR_NODATA, "%s:%04x: Attribute has no data!", attr->chunk->tag, attr->id);
    }
    CHECK_ERR(seek_to_pos(omrx, attr->file_pos));
    //FIXME: deal with array datatypes, non-raw encodings
    if (attr->datatype == OMRX_DTYPE_UTF8) {
        attr->data = malloc(attr->size + 1);
        CHECK_ERR(read_data(omrx, attr->size, attr->data));
        ((char *)attr->data)[attr->size] = 0;
    } else {
        attr->data = malloc(attr->size);
        CHECK_ALLOC(omrx, attr->data);
        CHECK_ERR(read_data(omrx, attr->size, attr->data));
    }

    return OMRX_OK;
}

static omrx_status_t release_attr_data(omrx_attr_t attr) {
    if (!attr->data) {
        return OMRX_OK;
    }
    if (attr->file_pos < 0) {
        // This isn't a file-backed attribute.  Do nothing.
        return OMRX_RESULT_NOT_FOUND;
    }
    free(attr->data);

    return OMRX_OK;
}

static omrx_status_t freeze_attr_data(omrx_attr_t attr) {
    if (attr->file_pos < 0) {
        // Not a file-backed attribute (or already frozen).  Do nothing.
        return OMRX_RESULT_NOT_FOUND;
    }
    if (!attr->data) {
        // Not currently loaded.  Load it first.
        CHECK_ERR(load_attr_data(attr));
    }
    // Now "freeze" it by removing its file backing info, so it can't be
    // released later.
    attr->file_pos = -1;

    return OMRX_OK;
}

static omrx_status_t find_attr(omrx_chunk_t chunk, uint16_t id, omrx_attr_t *dest) {
    omrx_attr_t attr = chunk->attrs;

    while (attr) {
        if (attr->id == id) {
            *dest = attr;
            return OMRX_OK;
        }
        attr = attr->next;
    }

    *dest = NULL;
    return OMRX_RESULT_NOT_FOUND;
}

static omrx_status_t chunk_add_attr(omrx_chunk_t chunk, omrx_attr_t attr) {
    omrx_attr_t ptr = (omrx_attr_t)(&chunk->attrs);

    while (ptr->next) {
        if (ptr->next->id > attr->id) break;
        ptr = ptr->next;
    }
    attr->next = ptr->next;
    ptr->next = attr;
    chunk->attr_count += 1;

    return OMRX_OK;
}

static omrx_status_t add_child_chunk(omrx_chunk_t parent, omrx_chunk_t child) {
    child->parent = parent;
    if (!parent->first_child) {
        parent->first_child = child;
        parent->last_child = child;
    } else {
        parent->last_child->next = child;
        parent->last_child = child;
    }

    return OMRX_OK;
}

static omrx_status_t omrx_scan(omrx_t omrx) {
    off_t file_pos;
    uint8_t tag[4];
    uint32_t ver;

    file_pos = ftello(omrx->fp);
    if (file_pos < 0) {
        return omrx_os_error(omrx, "Cannot read file position");
    }
    CHECK_ERR(read_data(omrx, 4, &tag));
    if (memcmp(tag, "OMRX", 4)) {
        return omrx_error(omrx, OMRX_ERR_BAD_MAGIC, "Bad data at beginning of file (not an OMRX file?)");
    }
    //FIXME: check whether we've already been read/populated before
    CHECK_ERR(seek_to_pos(omrx, file_pos));
    if (omrx->top_chunk) {
        free_all_chunks(omrx->top_chunk);
        omrx->top_chunk = NULL;
    }
    CHECK_ERR(read_next_chunk(omrx));
    CHECK_ERR(omrx_get_version(omrx, &ver));
    if (ver > OMRX_VERSION) {
        if (OMRX_VER_MAJOR(ver) > OMRX_VER_MAJOR(OMRX_VERSION)) {
            return omrx_error(omrx, OMRX_ERR_BAD_VER, "File version (%d.%d) is unsupported by this software (software version is %d.%d).", OMRX_VER_MAJOR(ver), OMRX_VER_MINOR(ver), OMRX_VER_MAJOR(OMRX_VERSION), OMRX_VER_MINOR(OMRX_VERSION));
        } else {
            omrx_warning(omrx, OMRX_WARN_BAD_VER, "File version (%d.%d) is greater than supported version (%d.%d).  Some features may be unavailable.", OMRX_VER_MAJOR(ver), OMRX_VER_MINOR(ver), OMRX_VER_MAJOR(OMRX_VERSION), OMRX_VER_MINOR(OMRX_VERSION));
        }
    }
    while (omrx->context) {
        CHECK_ERR(read_next_chunk(omrx));
    }

    return OMRX_OK;
}

static omrx_status_t read_next_chunk(omrx_t omrx) {
    struct chunk_header hdr;
    struct attr_header attr_hdr;
    omrx_chunk_t chunk;
    omrx_attr_t attr;
    off_t file_pos;
    uint32_t tagint;
    uint_fast16_t i;
    uint_fast16_t attr_count;

    CHECK_ERR(read_data(omrx, CHUNKHDR_SIZE, &hdr));
    file_pos = ftello(omrx->fp);
    if (file_pos < 0) {
        return omrx_os_error(omrx, "Cannot read file position");
    }
    tagint = TAG_TO_TAGINT(hdr.tag);
    hdr.count = UINT16_FTOH(hdr.count);
    // Make sure it looks like a valid tag and we're not just reading garbage
    // at this point (each byte should be in the range 0x40-0x7f)
    if ((tagint & 0xc0c0c0c0) != 0x40404040) {
        return omrx_error(omrx, OMRX_ERR_BAD_CHUNK, "Invalid chunk tag found (%08x). File likely corrupted.", tagint);
    }

    chunk = new_chunk(omrx, hdr.tag);
    CHECK_ALLOC(omrx, chunk);
    chunk->file_position = file_pos;
    attr_count = UINT16_FTOH(hdr.count);

    for (i=0; i < attr_count; i++) {
        CHECK_ERR(read_data(omrx, ATTRHDR_SIZE, &attr_hdr));
        attr_hdr.id = UINT16_FTOH(attr_hdr.id);
        attr_hdr.datatype = UINT16_FTOH(attr_hdr.datatype);
        attr_hdr.size = UINT32_FTOH(attr_hdr.size);
        file_pos = ftello(omrx->fp);
        if (file_pos < 0) {
            return omrx_os_error(omrx, "Cannot read file position");
        }
        attr = new_attr(chunk, attr_hdr.id, attr_hdr.datatype, attr_hdr.size, file_pos);
        CHECK_ALLOC(omrx, attr);

        if (OMRX_IS_ARRAY_DTYPE(attr_hdr.datatype)) {
            CHECK_ERR(read_attr_subheader_array(attr));
        }

        if (attr_hdr.id == OMRX_ATTR_ID) {
            //FIXME: an error here isn't necessarily a fatal error
            if (attr_hdr.datatype == OMRX_DTYPE_UTF8) {
                CHECK_ERR(load_attr_data(attr));
                CHECK_ERR(freeze_attr_data(attr));
                chunk->id = attr->data;
            } else {
                omrx_warning(omrx, OMRX_WARN_BAD_ATTR, "%s:id attribute has wrong type (%04x).  Ignored.", &chunk->tag, attr_hdr.datatype);
                CHECK_ERR(skip_data(omrx, attr->size));
            }
        } else {
            CHECK_ERR(skip_data(omrx, attr->size));
        }
        CHECK_ERR(chunk_add_attr(chunk, attr));
    }
    //TODO: check for a toplevel critical tag and take appropriate action
    if (!omrx->context) {
        // This is the first (OMRX) chunk.  Set it up as the toplevel chunk.
        omrx->top_chunk = chunk;
        omrx->context = chunk;
    } else {
        if (tagint == (omrx->context->tagint | END_CHUNK_FLAG)) {
            // End tag for our current context.  Pop a nesting level.
            omrx->context = omrx->context->parent;
        } else {
            CHECK_ERR(add_child_chunk(omrx->context, chunk));
        }
        if (!(tagint & END_CHUNK_FLAG)) {
            // This is a start tag.  Push a nesting level onto our context.
            omrx->context = chunk;
        }
    }
    return OMRX_OK;
}

static omrx_status_t read_attr_subheader_array(omrx_attr_t attr) {
    omrx_t omrx = attr->chunk->omrx;

    if (attr->size < 2) {
        omrx_warning(omrx, OMRX_WARN_BAD_ATTR, "%s:%04x attribute has bad length.", attr->chunk->tag, attr->id);
        CHECK_ERR(skip_data(omrx, attr->size));
        attr->size = 0;
        return OMRX_WARN_BAD_ATTR;
    }
    CHECK_ERR(read_data(omrx, 2, &attr->cols));
    attr->cols = UINT16_FTOH(attr->cols);
    if (!attr->cols) {
        attr->cols = 1;
    }
    attr->file_pos += 2;
    attr->size -= 2;

    return OMRX_OK;
}

static omrx_status_t write_chunk(omrx_chunk_t chunk, FILE *fp) {
    omrx_t omrx = chunk->omrx;
    struct chunk_header hdr;
    omrx_attr_t attr;
    omrx_chunk_t child;

    memcpy(hdr.tag, chunk->tag, 4);
    hdr.count = UINT16_HTOF(chunk->attr_count);
    CHECK_ERR(write_data(omrx, sizeof(hdr), &hdr, fp));
    attr = chunk->attrs;
    while (attr) {
        CHECK_ERR(write_attr(attr, fp));
        attr = attr->next;
    }
    if (!(chunk->tagint & END_CHUNK_FLAG)) {
        // FIXME: make this non-recursive
        child = chunk->first_child;
        while (child) {
            CHECK_ERR(write_chunk(child, fp));
            child = child->next;
        }
        // Write close-tag
        hdr.tag[3] |= CHUNK_TAG_FLAG;
        hdr.count = 0;
        CHECK_ERR(write_data(omrx, sizeof(hdr), &hdr, fp));
    }

    return OMRX_OK;
}

static omrx_status_t write_attr_subheader_array(omrx_attr_t attr, FILE *fp) {
    uint16_t cols = UINT16_HTOF(attr->cols);

    return write_data(attr->chunk->omrx, 2, &cols, fp);
}

static omrx_status_t write_attr(omrx_attr_t attr, FILE *fp) {
    omrx_t omrx = attr->chunk->omrx;
    struct attr_header hdr;
    bool do_release = false;

    hdr.id = UINT16_HTOF(attr->id);
    hdr.datatype = UINT16_HTOF(attr->datatype);

    if (!attr->data) {
        // We need to load the data before we can write it out again
        CHECK_ERR(load_attr_data(attr));
        do_release = true;
    }
    if (OMRX_IS_ARRAY_DTYPE(attr->datatype)) {
        hdr.size = UINT32_HTOF(attr->size + 2);
        CHECK_ERR(write_data(omrx, sizeof(hdr), &hdr, fp));
        CHECK_ERR(write_attr_subheader_array(attr, fp));
    } else {
        hdr.size = UINT32_HTOF(attr->size);
        CHECK_ERR(write_data(omrx, sizeof(hdr), &hdr, fp));
    }
    // FIXME: endianness of data, encoding, etc
    CHECK_ERR(write_data(omrx, attr->size, attr->data, fp));
    if (do_release) {
        CHECK_ERR(release_attr_data(attr));
    }

    return OMRX_OK;
}

static uint32_t get_elem_size(uint16_t dtype, uint32_t total_size) {
    if (OMRX_IS_SIMPLE_DTYPE(dtype) || OMRX_IS_ARRAY_DTYPE(dtype)) {
        // For simple and array types, the low two bits always indicate the
        // element width.
        return OMRX_GET_ELEMSIZE(dtype);
    }
    switch (dtype) {
        case OMRX_DTYPE_UTF8:
        case OMRX_DTYPE_RAW:
            return total_size;
    }

    // We don't know about this type.  The only thing we can do is return 0.
    return 0;
}

/////////////// External Chunk API ///////////////////

bool do_omrx_init(int api_ver) {
    if (api_ver != OMRX_API_VER) {
        return false;
    }
    return true;
}

omrx_t omrx_new(void) {
    omrx_t omrx = malloc(sizeof(struct omrx));

    if (!omrx) {
        return NULL;
    }
    omrx->message = malloc(OMRX_ERRMSG_BUFSIZE);
    if (!omrx->message) {
        // FIXME: print an error message
        omrx_free(omrx);
        return NULL;
    }
    omrx->log_error = default_log_error;
    omrx->log_warning = default_log_warning;
    omrx->top_chunk = new_chunk(omrx, "OMRX");
    if (!omrx->top_chunk) {
        // FIXME: print an error message
        omrx_free(omrx);
        return NULL;
    }
    if (omrx_set_attr_uint32(omrx->top_chunk, OMRX_ATTR_VER, OMRX_MIN_VERSION) < 0) {
        omrx_free(omrx);
        return NULL;
    }
    omrx->status = OMRX_OK;
    omrx->last_result = OMRX_OK;

    return omrx;
}

omrx_status_t omrx_free(omrx_t omrx) {
    omrx_status_t status = OMRX_OK;
    omrx_status_t rc;

    // Note: we don't exit immediately on errors in here, because we want to
    // continue on and clean up as much as possible anyway.  We save any error
    // status and return it at the end of things.
    if (omrx->fp) {
        rc = omrx_close(omrx);
        if (rc != OMRX_OK) status = rc;
    }
    if (omrx->filename) {
        free(omrx->filename);
    }
    if (omrx->top_chunk) {
        rc = free_all_chunks(omrx->top_chunk);
        if (rc != OMRX_OK) status = rc;
    }
    if (omrx->message) {
        free(omrx->message);
    }
    free(omrx);

    return status;
}

omrx_status_t omrx_status(omrx_t omrx, bool reset) {
    omrx_status_t status = omrx->status;

    if (reset) {
        omrx->status = OMRX_OK;
    }
    return status;
}

omrx_status_t omrx_last_result(omrx_t omrx) {
    return omrx->last_result;
}

omrx_status_t omrx_get_version(omrx_t omrx, uint32_t *ver) {
    return omrx_get_attr_uint32(omrx->top_chunk, OMRX_ATTR_VER, ver);
}

omrx_status_t omrx_open(omrx_t omrx, const char *filename) {
    if (omrx->fp) {
        return omrx_error(omrx, OMRX_ERR_ALREADY_OPEN, "omrx_open() called on already open OMRX handle");
    }
    omrx->filename = strdup(filename);
    omrx->fp = fopen(filename, "rb");
    if (!omrx->fp) {
        return omrx_os_error(omrx, "Cannot open %s for reading", filename);
    }

    return omrx_scan(omrx);
}

omrx_status_t omrx_close(omrx_t omrx) {
    if (!omrx->fp) {
        return omrx_error(omrx, OMRX_ERR_NOT_OPEN, "omrx_close() called on non-open OMRX handle");
    }
    if (fclose(omrx->fp)) {
        omrx->fp = NULL;
        return omrx_os_error(omrx, "Close failed");
    }
    omrx->fp = NULL;

    return RESULT(omrx, OMRX_OK);
}

omrx_chunk_t omrx_get_root_chunk(omrx_t omrx) {
    return omrx->top_chunk;
}

omrx_status_t omrx_get_child(omrx_chunk_t chunk, const char *tag, omrx_chunk_t *result) {
    if (!chunk) return OMRX_RESULT_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
   
    if (tag) {
        uint32_t tagint = TAG_TO_TAGINT(tag);

        chunk = chunk->first_child;
        while (chunk) {
            if (chunk->tagint == tagint) {
                *result = chunk;
                return RESULT(omrx, OMRX_OK);
            }
            chunk = chunk->next;
        }
    } else if (chunk->first_child) {
        *result = chunk->first_child;
        return RESULT(omrx, OMRX_OK);
    }
    *result = NULL;
    return RESULT(omrx, OMRX_RESULT_NOT_FOUND);
}

omrx_status_t omrx_get_next_chunk(omrx_chunk_t chunk, const char *tag, omrx_chunk_t *result) {
    if (!chunk) return OMRX_RESULT_NO_OBJECT;

    omrx_t omrx = chunk->omrx;

    if (tag) {
        uint32_t tagint = chunk->tagint;

        while (chunk->next) {
            chunk = chunk->next;
            if (chunk->tagint == tagint) {
                *result = chunk;
                return RESULT(omrx, OMRX_OK);
            }
        }
    } else if (chunk->next) {
        *result = chunk->next;
        return RESULT(omrx, OMRX_OK);
    }

    *result = NULL;
    return RESULT(omrx, OMRX_RESULT_NOT_FOUND);
}

omrx_status_t omrx_get_child_by_id(omrx_chunk_t chunk, const char *tag, const char *id, omrx_chunk_t *result) {
    if (!chunk) return OMRX_RESULT_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    uint32_t tagint;
   
    if (tag) {
        tagint = TAG_TO_TAGINT(tag);
    } else {
        tagint = 0;
    }

    chunk = chunk->first_child;
    while (chunk) {
        if (!tagint || (tagint == chunk->tagint)) {
            if (chunk->id && !strcmp(chunk->id, id)) {
                *result = chunk;
                return RESULT(omrx, OMRX_OK);
            }
        }
        chunk = chunk->next;
    }
    *result = NULL;
    return RESULT(omrx, OMRX_RESULT_NOT_FOUND);
}

omrx_status_t omrx_add_chunk(omrx_chunk_t chunk, const char *tag, omrx_chunk_t *result) {
    if (!chunk) return OMRX_RESULT_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_chunk_t child = new_chunk(omrx, tag);

    CHECK_ALLOC(omrx, child);
    CHECK_ERR(add_child_chunk(chunk, child));
    if (result) {
        *result = child;
    }

    return RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_del_chunk(omrx_chunk_t chunk) {
    if (!chunk) return OMRX_RESULT_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_chunk_t sibling;

    sibling = (omrx_chunk_t)(&chunk->parent->first_child);
    while (sibling) {
        if (sibling->next == chunk) {
            sibling->next = chunk->next;
            if (chunk == chunk->parent->last_child) {
                chunk->parent->last_child = sibling;
            }
            break;
        }
    }
    CHECK_ERR(free_chunk(chunk));

    return RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_set_attr_str(omrx_chunk_t chunk, uint16_t id, omrx_ownership_t own, char *str) {
    if (!chunk) return OMRX_RESULT_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        attr = new_attr(chunk, id, OMRX_DTYPE_UTF8, 0, -1);
        CHECK_ALLOC(omrx, attr);
        CHECK_ERR(chunk_add_attr(chunk, attr));
    }
    if (attr->datatype != OMRX_DTYPE_UTF8) {
        return omrx_error(omrx, OMRX_ERR_BAD_DTYPE, "Attempt to set string value for non-string attribute %s:%04x (type=%04x).", chunk->tag, id, attr->datatype);
    }
    if (attr->data) {
        // FIXME: do we need to worry about people with refs to this?
        free(attr->data);
    }
    if (own == OMRX_COPY) {
        attr->data = strdup(str);
        CHECK_ALLOC(omrx, attr->data);
    } else {
        attr->data = str;
    }
    attr->size = strlen(attr->data);

    return RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_get_attr_info(omrx_chunk_t chunk, uint16_t id, struct omrx_attr_info *info) {
    if (!chunk) {
        info->exists = false;
        return OMRX_RESULT_NO_OBJECT;
    }

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        info->exists = false;
        return RESULT(omrx, OMRX_RESULT_NOT_FOUND);
    }
    info->exists = true;
    info->encoded_type = attr->datatype;
    info->size = attr->size;
    info->elem_size = get_elem_size(attr->datatype, attr->size);
    if (OMRX_IS_ARRAY_DTYPE(attr->datatype)) {
        info->elem_type = OMRX_GET_ELEMTYPE(attr->datatype);
        info->is_array = true;
        info->cols = attr->cols;
        if (info->elem_size) {
            info->rows = (attr->size / attr->cols) / info->elem_size;
        } else {
            // We don't know the intrinsic size of this type, so we can't
            // calculate the number of rows.
            info->rows = 0;
        }
    } else {
        info->elem_type = info->encoded_type;
        info->is_array = false;
        info->cols = 1;
        info->rows = 1;
        if (!info->elem_size) {
            // We don't know the size of this type.  Since we're not an array,
            // just assume that it takes up the whole size of the data portion.
            info->elem_size = info->size;
        }
    }

    return RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_get_attr_str(omrx_chunk_t chunk, uint16_t id, char **dest) {
    if (!chunk) return OMRX_RESULT_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        *dest = NULL;
        return RESULT(omrx, OMRX_RESULT_NOT_FOUND);
    }
    if (attr->datatype != OMRX_DTYPE_UTF8) {
        return omrx_error(omrx, OMRX_ERR_BAD_DTYPE, "Attempt to get string value of non-string attribute %s:%04x (type=%04x).", chunk->tag, id, attr->datatype);
    }
    if (!attr->data) {
        CHECK_ERR(load_attr_data(attr));
    }
    *dest = attr->data;

    return RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_set_attr_uint32(omrx_chunk_t chunk, uint16_t id, uint32_t value) {
    if (!chunk) return OMRX_RESULT_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        attr = new_attr(chunk, id, OMRX_DTYPE_U32, 4, -1);
        CHECK_ALLOC(omrx, attr);
        CHECK_ERR(chunk_add_attr(chunk, attr));
    }
    if (attr->datatype != OMRX_DTYPE_U32) {
        return omrx_error(omrx, OMRX_ERR_BAD_DTYPE, "Attempt to set uint32 value for non-uint32 attribute %s:%04x (type=%04x).", chunk->tag, id, attr->datatype);
    }
    if (!attr->data) {
        attr->data = malloc(4);
        CHECK_ALLOC(omrx, attr->data);
    }
    *((uint32_t *)attr->data) = value;

    return RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_get_attr_uint32(omrx_chunk_t chunk, uint16_t id, uint32_t *dest) {
    if (!chunk) return OMRX_RESULT_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        *dest = 0;
        return RESULT(omrx, OMRX_RESULT_NOT_FOUND);
    }
    if (attr->datatype != OMRX_DTYPE_U32) {
        return omrx_error(omrx, OMRX_ERR_BAD_DTYPE, "Attempt to get uint32 value of non-uint32 attribute %s:%04x (type=%04x).", chunk->tag, id, attr->datatype);
    }
    if (!attr->data) {
        CHECK_ERR(load_attr_data(attr));
    }
    *dest = *((uint32_t *)attr->data);

    return RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_set_attr_float32_array(omrx_chunk_t chunk, uint16_t id, omrx_ownership_t own, uint16_t cols, uint32_t rows, float *data) {
    if (!chunk) return OMRX_RESULT_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        attr = new_attr(chunk, id, OMRX_DTYPE_F32_ARRAY, 0, -1);
        CHECK_ALLOC(omrx, attr);
        CHECK_ERR(chunk_add_attr(chunk, attr));
    }
    if (attr->datatype != OMRX_DTYPE_F32_ARRAY) {
        return omrx_error(omrx, OMRX_ERR_BAD_DTYPE, "Attempt to set float-array value for non-float-array attribute %s:%04x (type=%04x).", chunk->tag, id, attr->datatype);
    }
    if (attr->data) {
        free(attr->data);
    }
    attr->size = 4 * rows * cols;
    attr->cols = cols;
    if (own == OMRX_COPY) {
        attr->data = malloc(attr->size);
        CHECK_ALLOC(omrx, attr->data);
        memcpy(attr->data, data, attr->size);
    } else {
        attr->data = data;
    }

    return RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_get_attr_float32_array(omrx_chunk_t chunk, uint16_t id, float **dest) {
    if (!chunk) return OMRX_RESULT_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        *dest = 0;
        return RESULT(omrx, OMRX_RESULT_NOT_FOUND);
    }
    if (attr->datatype != OMRX_DTYPE_F32_ARRAY) {
        return omrx_error(omrx, OMRX_ERR_BAD_DTYPE, "Attempt to get float32-array value of non-float32-array attribute %s:%04x (type=%04x).", chunk->tag, id, attr->datatype);
    }
    if (!attr->data) {
        CHECK_ERR(load_attr_data(attr));
    }
    *dest = (float *)attr->data;

    return RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_release_attr_data(omrx_chunk_t chunk, uint16_t id) {
    if (!chunk) return OMRX_RESULT_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    //FIXME: implement this
    return RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_del_attr(omrx_chunk_t chunk, uint16_t id) {
    if (!chunk) return OMRX_RESULT_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t prev = (omrx_attr_t)(&chunk->attrs);
    omrx_attr_t attr;

    while (prev->next) {
        if (prev->next->id == id) {
            attr = prev->next;
            prev->next = attr->next;
            CHECK_ERR(free_attr(attr));
            return RESULT(omrx, OMRX_OK);
        }
        prev = prev->next;
    }

    return RESULT(omrx, OMRX_RESULT_NOT_FOUND);
}

omrx_status_t omrx_write(omrx_t omrx, const char *filename) {
    FILE *fp = fopen(filename, "wb");

    if (!fp) {
        return omrx_os_error(omrx, "Cannot open %s for writing", filename);
    }
    CHECK_ERR(write_chunk(omrx->top_chunk, fp));

    if (fclose(fp)) {
        return omrx_os_error(omrx, "Close failed");
    }

    return RESULT(omrx, OMRX_OK);
}

