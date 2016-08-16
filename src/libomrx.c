#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "omrx.h"
#include "omrx_internal.h"

/** @cond internal
  */

//TODO: make sure zero-length arrays are handled properly (malloc, read/write, etc)

#define LOG(...) fprintf(stderr, __VA_ARGS__); fflush(stderr);

#ifdef LOGIO
  #define LOG_IO(...) LOG(__VA_ARGS__)
#else
  #define LOG_IO(...) /* do nothing */
#endif

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

static void *omrx_default_alloc(omrx_t omrx, size_t size);
static void omrx_default_free(omrx_t omrx, void *ptr);
static char *omrx_strdup(omrx_t omrx, const char *s);

static omrx_status_t seek_to_pos(omrx_t omrx, off_t pos);
static omrx_status_t skip_data(omrx_t omrx, off_t size);
static omrx_status_t read_data(omrx_t omrx, off_t size, void *dest);
static omrx_status_t write_data(omrx_t omrx, off_t size, const void *src, FILE *fp);

static omrx_chunk_t new_chunk(omrx_t omrx, const char *tag);
static omrx_status_t free_chunk(omrx_chunk_t chunk);
static omrx_status_t free_all_chunks(omrx_chunk_t chunk);
static omrx_attr_t new_attr(omrx_chunk_t chunk, uint16_t id, uint16_t datatype, uint32_t size, off_t file_pos);
static omrx_status_t free_attr(omrx_attr_t attr);

static omrx_status_t load_attr_data(omrx_attr_t attr, void **dest);
static omrx_status_t release_attr_data(omrx_attr_t attr);
static omrx_status_t find_attr(omrx_chunk_t chunk, uint16_t id, omrx_attr_t *dest);
static omrx_status_t chunk_add_attr(omrx_chunk_t chunk, omrx_attr_t attr);
static omrx_status_t add_child_chunk(omrx_chunk_t parent, omrx_chunk_t child);
static omrx_status_t register_chunk_id(omrx_chunk_t chunk, char *idstr);
static omrx_status_t deregister_chunk_id(omrx_chunk_t chunk);
static omrx_status_t lookup_chunk_id(omrx_t omrx, const char *idstr, omrx_chunk_t *result);
static omrx_status_t omrx_scan(omrx_t omrx);
static omrx_status_t read_next_chunk(omrx_t omrx);
static omrx_status_t read_attr_subheader_array(omrx_attr_t attr);
static omrx_status_t write_chunk(omrx_chunk_t chunk, FILE *fp);
static omrx_status_t write_attr_subheader_array(omrx_attr_t attr, FILE *fp);
static omrx_status_t write_attr(omrx_attr_t attr, FILE *fp);
static uint32_t get_elem_size(uint16_t dtype, uint32_t total_size);

///////////////////////////////////////////////

static omrx_log_func_t default_log_warning = NULL;
static omrx_log_func_t default_log_error = NULL;
static omrx_alloc_func_t default_alloc = omrx_default_alloc;
static omrx_free_func_t default_free = omrx_default_free;

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

omrx_status_t omrx_os_warning(omrx_t omrx, omrx_status_t errcode, const char *fmt, ...) {
    va_list ap;
    size_t msglen;

    va_start(ap, fmt);
    if (vsnprintf(omrx->message, OMRX_ERRMSG_BUFSIZE, fmt, ap) < 0) {
        strncpy(omrx->message, "(unable to format warning message)", OMRX_ERRMSG_BUFSIZE);
        omrx->message[OMRX_ERRMSG_BUFSIZE - 1] = 0;
    }
    va_end(ap);

    msglen = strlen(omrx->message);
    // If there's space, tack on the strerror() message after our own message.
    if (msglen < OMRX_ERRMSG_BUFSIZE - 3) {
        omrx->message[msglen] = ':';
        omrx->message[msglen + 1] = ' ';
        if (strerror_r(errno, omrx->message + msglen + 2, OMRX_ERRMSG_BUFSIZE - msglen - 2)) {
            strncpy(omrx->message + msglen + 2, "(strerror failed)", OMRX_ERRMSG_BUFSIZE - msglen - 2);
            omrx->message[OMRX_ERRMSG_BUFSIZE - 1] = 0;
        }
    }

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

omrx_status_t omrx_os_error(omrx_t omrx, omrx_status_t errcode, const char *fmt, ...) {
    va_list ap;
    size_t msglen;

    if (errno == 0 && omrx->fp && feof(omrx->fp)) {
        // We must have gotten here because of a short read due to hitting
        // EOF unexpectedly.
        errcode = OMRX_ERR_EOF;
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
            // Since errno=0, strerror normally returns the (rather confusing
            // and not useful) "No error" message for this.  Use a better
            // message in this case.
            strncpy(omrx->message + msglen + 2, "Unexpected end of file", OMRX_ERRMSG_BUFSIZE - msglen - 2);
            omrx->message[OMRX_ERRMSG_BUFSIZE - 1] = 0;
        } else {
            if (strerror_r(errno, omrx->message + msglen + 2, OMRX_ERRMSG_BUFSIZE - msglen - 2)) {
                strncpy(omrx->message + msglen + 2, "(strerror failed)", OMRX_ERRMSG_BUFSIZE - msglen - 2);
                omrx->message[OMRX_ERRMSG_BUFSIZE - 1] = 0;
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

///////////////////////////////////

static void *omrx_default_alloc(omrx_t omrx, size_t size) {
    void *ptr = malloc(size);
    //LOG("- malloc %p\n", ptr);
    return ptr;
}

static void omrx_default_free(omrx_t omrx, void *ptr) {
    //LOG("- free %p\n", ptr);
    free(ptr);
}

static char *omrx_strdup(omrx_t omrx, const char *s) {
    size_t size = strlen(s);
    char *dup = omrx->alloc(omrx, size);

    if (!dup) return dup;

    return strcpy(dup, s);
}

static omrx_status_t seek_to_pos(omrx_t omrx, off_t pos) {
    LOG_IO("- seek %lu\n", pos);
    if (fseeko(omrx->fp, pos, SEEK_SET) < 0) {
        return omrx_os_error(omrx, OMRX_ERR_OSERR, "Seek failed");
    }

    return OMRX_OK;
}

static omrx_status_t skip_data(omrx_t omrx, off_t size) {
    LOG_IO("- skip %lu\n", size);
    if (fseeko(omrx->fp, size, SEEK_CUR) < 0) {
        return omrx_os_error(omrx, OMRX_ERR_OSERR, "Seek failed");
    }

    return OMRX_OK;
}

static omrx_status_t read_data(omrx_t omrx, off_t size, void *dest) {
    if (fread(dest, size, 1, omrx->fp) != 1) {
        return omrx_os_error(omrx, OMRX_ERR_OSERR, "Read error");
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
        return omrx_os_error(omrx, OMRX_ERR_OSERR, "Write error");
    }

    return OMRX_OK;
}

///////////////////////////////////

static omrx_chunk_t new_chunk(omrx_t omrx, const char *tag) {
    omrx_chunk_t chunk;

    chunk = omrx->alloc(omrx, sizeof(struct omrx_chunk));
    if (!chunk) return NULL;
    memset(chunk, 0, sizeof(struct omrx_chunk));

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
    omrx_t omrx = chunk->omrx;
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
    if (chunk->id) {
        omrx->free(omrx, chunk->id);
    }
    omrx->free(omrx, chunk);

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
    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr;

    attr = omrx->alloc(omrx, sizeof(struct omrx_attr));
    if (!attr) return NULL;
    memset(attr, 0, sizeof(struct omrx_attr));

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
    omrx_t omrx = attr->chunk->omrx;

    if (attr->data) {
        // FIXME: need to check if anybody's using it still
        omrx->free(omrx, attr->data);
    }
    omrx->free(omrx, attr);

    return OMRX_OK;
}

// Note: it is important that this function leaves the file pointer after the
// last byte of the read data as a couple of other things (i.e.
// read_next_chunk) rely on that.
static omrx_status_t load_attr_data(omrx_attr_t attr, void **dest) {
    omrx_t omrx = attr->chunk->omrx;
    omrx_status_t status;

    if (attr->data) {
        // Attribute is not file backed or has locally-modified value.  Just
        // copy what's in memory.
        if (attr->datatype == OMRX_DTYPE_UTF8) {
            // For strings, make sure there's a zero-byte at the end.
            *dest = omrx->alloc(omrx, attr->size + 1);
            CHECK_ALLOC(omrx, *dest);
            memcpy(*dest, attr->data, attr->size);
            ((char *)(*dest))[attr->size] = 0;
        } else {
            *dest = omrx->alloc(omrx, attr->size);
            CHECK_ALLOC(omrx, *dest);
            memcpy(*dest, attr->data, attr->size);
        }
        return OMRX_OK;
    }
    if (attr->file_pos < 0) {
        // We called load_attr_data on a non-file-backed attribute.
        // This is probably because we created a new (not read from a
        // file) attribute and forgot to assign data to it.
        return omrx_error(omrx, OMRX_ERR_INTERNAL, "%s:%04x: Attempt to read from non-file-backed attribute!", attr->chunk->tag, attr->id);
    }
    CHECK_ERR(seek_to_pos(omrx, attr->file_pos));
    //FIXME: deal with non-raw encodings
    if (attr->datatype == OMRX_DTYPE_UTF8) {
        // For strings, make sure there's a zero-byte at the end.
        *dest = omrx->alloc(omrx, attr->size + 1);
        CHECK_ALLOC(omrx, *dest);
        status = read_data(omrx, attr->size, *dest);
        if (status < 0) {
            omrx->free(omrx, *dest);
            *dest = NULL;
            return status;
        }
        ((char *)(*dest))[attr->size] = 0;
    } else {
        *dest = omrx->alloc(omrx, attr->size);
        CHECK_ALLOC(omrx, *dest);
        status = read_data(omrx, attr->size, *dest);
        if (status < 0) {
            omrx->free(omrx, *dest);
            *dest = NULL;
            return status;
        }
    }

    return OMRX_OK;
}

static omrx_status_t release_attr_data(omrx_attr_t attr) {
    omrx_t omrx = attr->chunk->omrx;

    if (!attr->data) {
        return OMRX_OK;
    }
    if (attr->file_pos < 0) {
        // This isn't a file-backed attribute.  Do nothing.
        return OMRX_STATUS_NOT_FOUND;
    }
    omrx->free(omrx, attr->data);

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
    return OMRX_STATUS_NOT_FOUND;
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
        return omrx_os_error(omrx, OMRX_ERR_OSERR, "Cannot read file position");
    }
    CHECK_ERR(read_data(omrx, 4, &tag));
    if (memcmp(tag, "OMRX", 4)) {
        return omrx_error(omrx, OMRX_ERR_BAD_MAGIC, "Bad data at beginning of file (not an OMRX file?)");
    }
    //FIXME: check whether we've already been read/populated before
    CHECK_ERR(seek_to_pos(omrx, file_pos));
    if (omrx->root_chunk) {
        free_all_chunks(omrx->root_chunk);
        omrx->root_chunk = NULL;
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

static omrx_status_t register_chunk_id(omrx_chunk_t chunk, char *idstr) {
    omrx_t omrx = chunk->omrx;
    int i;
    int next_free = -1;
    struct idmap_st *new_id_map;

    if (chunk->id) {
        deregister_chunk_id(chunk);
    }
    chunk->id = idstr;

    for (i = 0; i < omrx->chunk_id_map_size; i++) {
        if (!omrx->chunk_id_map[i].id) {
            if (next_free == -1) {
                next_free = i;
            }
        } else if (!strcmp(omrx->chunk_id_map[i].id, idstr)) {
            // FIXME: should this be a warning?
            return OMRX_STATUS_DUP;
        }
    }
    if (next_free == -1) {
        // Current map is full, we need to expand it to make space.
        next_free = omrx->chunk_id_map_size;
        omrx->chunk_id_map_size *= 2; // FIXME: should have a max increment
        new_id_map = realloc(omrx->chunk_id_map, sizeof(struct idmap_st) * omrx->chunk_id_map_size);
        if (!new_id_map) {
            return omrx_os_error(omrx, OMRX_ERR_ALLOC, "Cannot expand lookup table for new chunk ID");
        }
        omrx->chunk_id_map = new_id_map;
    }
    omrx->chunk_id_map[next_free].id = idstr;
    omrx->chunk_id_map[next_free].chunk = chunk;

    return OMRX_OK;
}

static omrx_status_t deregister_chunk_id(omrx_chunk_t chunk) {
    omrx_t omrx = chunk->omrx;
    int i;

    if (chunk->id) {
        for (i = 0; i < omrx->chunk_id_map_size; i++) {
            if (omrx->chunk_id_map[i].id == chunk->id) {
                omrx->chunk_id_map[i].id = NULL;
                break;
            }
        }
        free(chunk->id);
        chunk->id = NULL;
    }

    return OMRX_OK;
}

static omrx_status_t lookup_chunk_id(omrx_t omrx, const char *idstr, omrx_chunk_t *result) {
    int i;

    for (i = 0; i < omrx->chunk_id_map_size; i++) {
        if (!strcmp(omrx->chunk_id_map[i].id, idstr)) {
            *result = omrx->chunk_id_map[i].chunk;
            return OMRX_OK;
        }
    }

    return OMRX_STATUS_NOT_FOUND;
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
    char *idstr;

    CHECK_ERR(read_data(omrx, CHUNKHDR_SIZE, &hdr));
    file_pos = ftello(omrx->fp);
    if (file_pos < 0) {
        return omrx_os_error(omrx, OMRX_ERR_OSERR, "Cannot read file position");
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
            return omrx_os_error(omrx, OMRX_ERR_OSERR, "Cannot read file position");
        }
        attr = new_attr(chunk, attr_hdr.id, attr_hdr.datatype, attr_hdr.size, file_pos);
        CHECK_ALLOC(omrx, attr);

        if (OMRX_IS_ARRAY_DTYPE(attr_hdr.datatype)) {
            CHECK_ERR(read_attr_subheader_array(attr));
        }

        if (attr_hdr.id == OMRX_ATTR_ID) {
            //FIXME: an error here isn't necessarily a fatal error
            if (attr_hdr.datatype == OMRX_DTYPE_UTF8) {
                CHECK_ERR(load_attr_data(attr, (void **)&idstr));
                CHECK_ERR(register_chunk_id(chunk, idstr));
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
        omrx->root_chunk = chunk;
        omrx->context = chunk;
    } else {
        if (tagint == (omrx->context->tagint | END_CHUNK_FLAG)) {
            // End tag for our current context.  Pop a nesting level.
            omrx->context = omrx->context->parent;
            CHECK_ERR(free_chunk(chunk));
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
    void *data;

    hdr.id = UINT16_HTOF(attr->id);
    hdr.datatype = UINT16_HTOF(attr->datatype);

    if (OMRX_IS_ARRAY_DTYPE(attr->datatype)) {
        hdr.size = UINT32_HTOF(attr->size + 2);
        CHECK_ERR(write_data(omrx, sizeof(hdr), &hdr, fp));
        CHECK_ERR(write_attr_subheader_array(attr, fp));
    } else {
        hdr.size = UINT32_HTOF(attr->size);
        CHECK_ERR(write_data(omrx, sizeof(hdr), &hdr, fp));
    }
    // FIXME: endianness of data, encoding, etc
    if (attr->data) {
        CHECK_ERR(write_data(omrx, attr->size, attr->data, fp));
    } else {
        // We need to load the data before we can write it out again
        CHECK_ERR(load_attr_data(attr, &data));
        CHECK_ERR(write_data(omrx, attr->size, data, fp));
        omrx->free(omrx, data);
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

/** @endcond */

/////////////// External Chunk API ///////////////////

/** @defgroup api The libomrx API
  *
  * @{
  */

/** @defgroup init Library Initialization and OMRX Instances
  *
  * @brief Setting up, obtaining OMRX handles, error logging, etc.
  *
  * @{
  */

/** @def omrx_init
  * @brief Convenience wrapper for omrx_initialize()
  *
  * The first call into libomrx from any application must be either omrx_init()
  * or omrx_initialize().  You can use omrx_initialize() if you wish to
  * override some of the default parameters, such as the default log handling
  * functions (see omrx_initialize() for details).  However, if default values
  * for these parameters are acceptable (as is often the case), omrx_init() can
  * be used as a simpler alternative.
  *
  * Using omrx_init() is equivalent to the following:
  * @code
  * omrx_initialize(OMRX_API_VER, omrx_default_log_warning, omrx_default_log_error, NULL, NULL)
  * @endcode
  *
  * omrx_init() takes no parameters.  Return values are the same as for omrx_initialize().
  *
  * @note Unlike other libomrx functions, if omrx_init() returns an error
  * result, no error message is logged.  It is up to the application to print
  * a descriptive error if appropriate, should initialization fail.
  */

/** @brief Initialize the libomrx library
  *
  * omrx_initialize() must be invoked before any other libomrx functions/macros
  * are used.  It performs initial setup of the library and allows specifying
  * functions to use for logging errors/warnings and allocating and freeing
  * memory.
  *
  * Alternately, if you want to use default values for these parameters, the
  * omrx_init() macro provides a simpler interface which can be used instead of
  * calling omrx_initialize() directly.
  *
  * @param[in] api_ver    A constant indicating the version of the API expected
  *                       by the application.  This must always be
  *                       ::OMRX_API_VER.
  * @param[in] warn_func  The function to be called when a warning message
  *                       needs to be issued.  Can be `NULL`, in which case no
  *                       logging of warnings will be attempted.
  * @param[in] err_func   The function to be called when an error message needs
  *                       to be issued.  Can be `NULL`, in which case no
  *                       logging of errors will be attempted.
  * @param[in] alloc_func Function to use when allocating memory.  Can be
  *                       `NULL`, in which case a default implementation will
  *                       be used which uses the standard C `malloc()`.
  * @param[in] free_func  Function to use when freeing memory.  Can be `NULL`,
  *                       in which case a default implementation will be used
  *                       which uses the standard C `free()`.
  *
  * @retval ::OMRX_OK         Library initialization successful
  * @retval ::OMRX_ERR_BADAPI Initialization failed: The passed API version does
  *                           not match the version the library was compiled
  *                           with.  (This generally indicates the application
  *                           was compiled with a different version of the
  *                           headers than the library)
  *
  * @note Unlike other libomrx functions, if omrx_initialize() returns an error
  * result, no error message is logged.  It is up to the application to print
  * a descriptive error if appropriate, should initialization fail.
  */
omrx_status_t omrx_initialize(int api_ver, omrx_log_func_t warn_func, omrx_log_func_t err_func, omrx_alloc_func_t alloc_func, omrx_free_func_t free_func) {
    if (api_ver != OMRX_API_VER) {
        return OMRX_ERR_BADAPI;
    }

    if (!alloc_func) {
        alloc_func = omrx_default_alloc;
    }
    if (!free_func) {
        free_func = omrx_default_free;
    }
    default_log_warning = warn_func;
    default_log_error = err_func;
    default_alloc = alloc_func;
    default_free = free_func;

    return OMRX_OK;
}

/** @brief Create a new (empty) OMRX instance and return its handle.
  *
  * Optionally, a pointer (to anything) can be provided via the `user_data`
  * parameter which will be associated with the new OMRX instance.  This
  * pointer is not used in any way by libomrx, but can be used to store
  * application-specific data which should be associated with the instance, and
  * can be retrieved later with omrx_user_data().
  *
  * @param[in] user_data  Optional pointer to arbitrary application data
  * @param[out] result    A handle to the OMRX instance created
  *
  * @retval ::OMRX_OK        Instance created successfully
  * @retval ::OMRX_ERR_ALLOC Creation failed due to lack of memory
  */
omrx_status_t omrx_new(void *user_data, omrx_t *result) {
    omrx_t omrx;

    if (!default_alloc) {
        // FIXME: call default log functions
        *result = NULL;
        return OMRX_ERR_INIT_FIRST;
    }

    omrx = default_alloc(NULL, sizeof(struct omrx));

    if (!omrx) {
        // FIXME: call default log functions
        *result = NULL;
        return OMRX_ERR_ALLOC;
    }
    memset(omrx, 0, sizeof(struct omrx));

    omrx->user_data = user_data;
    omrx->alloc = default_alloc;
    omrx->free = default_free;
    omrx->message = omrx->alloc(omrx, OMRX_ERRMSG_BUFSIZE);
    omrx->log_error = default_log_error;
    omrx->log_warning = default_log_warning;
    omrx->root_chunk = new_chunk(omrx, "OMRX");
    omrx->chunk_id_map_size = 32;
    omrx->chunk_id_map = omrx->alloc(omrx, sizeof(struct idmap_st) * omrx->chunk_id_map_size);
    if (omrx->chunk_id_map) {
        memset(omrx->chunk_id_map, 0, sizeof(struct idmap_st) * omrx->chunk_id_map_size);
    }
    omrx->status = OMRX_OK;
    omrx->last_result = OMRX_OK;

    if (!omrx->message || !omrx->root_chunk || !omrx->chunk_id_map) {
        // FIXME: print an error message
        omrx_free(omrx);
        *result = NULL;
        return OMRX_ERR_ALLOC;
    }

    if (omrx_set_attr_uint32(omrx->root_chunk, OMRX_ATTR_VER, OMRX_MIN_VERSION) < 0) {
        omrx_free(omrx);
        *result = NULL;
        return OMRX_ERR_ALLOC;
    }

    *result = omrx;
    return OMRX_OK;
}

/** @brief Release all resources associated with an OMRX handle
  *
  * After calling this function, all underlying resources associated with the
  * given OMRX instance are closed and all memory is freed (including the
  * instance itself).  The provided handle should not be used for any future
  * calls.
  *
  * @note If a `user_data` pointer was provided when calling omrx_new(), the
  * user data is *not* freed by this function.  It is up to the application to
  * clean up any related application data if required.
  *
  * @param[in] omrx The OMRX instance to release.
  *
  * @retval ::OMRX_OK  Instance freed successfully
  */
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
        omrx->free(omrx, omrx->filename);
    }
    if (omrx->message) {
        omrx->free(omrx, omrx->message);
    }
    if (omrx->root_chunk) {
        rc = free_all_chunks(omrx->root_chunk);
        if (rc != OMRX_OK) status = rc;
    }
    if (omrx->chunk_id_map) {
        omrx->free(omrx, omrx->chunk_id_map);
    }
    omrx->free(omrx, omrx);

    return status;
}

/** @brief Retrieve the user_data pointer provided when calling omrx_new()
  *
  * @param[in] omrx  The OMRX instance
  *
  * @returns The value provided as the `user_data` parameter when omrx_new()
  * was called.
  */
void *omrx_user_data(omrx_t omrx) {
    return omrx->user_data;
}

/** @brief Return the status code from the last libomrx call
  *
  * This function returns whatever status code was returned by the most recent
  * call to any libomrx function (except omrx_status()).  This is a convenience
  * so that a function's return value can be tested directly (i.e. in an `if`
  * statement) for error conditions, but then can also be retrieved again later
  * if the application wants more information about a non-error result.  For
  * example:
  *
  * @code
  *     #define CHECK_ERR(x) if ((x) < 0) exit(1);
  *
  *     CHECK_ERR(omrx_get_chunk_by_id(omrx, "test", NULL, &chunk));
  *
  *     if (omrx_last_result(omrx) == OMRX_STATUS_NOT_FOUND) {
  *         printf("Didn't find 'test' id\n");
  *     }
  * @endcode
  *
  * Note that this differs from omrx_status() in that omrx_last_result() only
  * returns the result of the last call, and will not report any errors or
  * warnings which might have happened previously.
  *
  * @param[in] omrx The OMRX instance to query
  *
  * @returns The same status code as was returned by the previous libomrx call
  */
omrx_status_t omrx_last_result(omrx_t omrx) {
    return omrx->last_result;
}

/** @brief Return the current (accumulated) error/warning status
  *
  * libomrx maintains a record of the last error or warning status produced by
  * any API call.  omrx_status() can be called to find out whether any calls
  * have produced any errors or warnings up to this point (even if the most
  * recent call was successful).  Optionally, the status can be reset before
  * making a series of calls, so that it can be checked (for example) at the
  * end to ensure no undetected problems were encountered.
  *
  * If you only want to get the returned status code from the most recent
  * libomrx call (regardless of any previous errors), see omrx_last_result()
  * instead.
  *
  * @param[in] omrx The OMRX instance to query
  * @param[in] reset Reset the status to OMRX_OK after reading
  *
  * @returns The last error/warning result from any previous libomrx call, or OMRX_OK if no errors or warnings have occurred since the last reset.
  */
omrx_status_t omrx_status(omrx_t omrx, bool reset) {
    omrx_status_t status = omrx->status;

    if (reset) {
        omrx->status = OMRX_OK;
    }
    return status;
}

/** @brief Default logging function for warning messages
  *
  * This is the warning log function passed to omrx_initialize() if the
  * omrx_init() convenience wrapper is used.  It will send all warning messages
  * to stderr, prefixed with "libomrx warning: ".
  */
void omrx_default_log_warning(omrx_t omrx, omrx_status_t errcode, const char *msg) {
    if (omrx && omrx->filename) {
        fprintf(stderr, "libomrx warning: %s: %s\n", omrx->filename, msg);
    } else {
        fprintf(stderr, "libomrx warning: %s\n", msg);
    }
    fflush(stderr);
}

/** @brief Default logging function for error messages
  *
  * This is the error log function passed to omrx_initialize() if the
  * omrx_init() convenience wrapper is used.  It will send all error messages
  * to stderr, prefixed with "libomrx error: ".
  */
void omrx_default_log_error(omrx_t omrx, omrx_status_t errcode, const char *msg) {
    if (omrx && omrx->filename) {
        fprintf(stderr, "libomrx error: %s: %s\n", omrx->filename, msg);
    } else {
        fprintf(stderr, "libomrx error: %s\n", msg);
    }
    fflush(stderr);
}

/** @} */

/** @defgroup fileio File Operations
  *
  * @brief Opening, reading, and writing OMRX files
  *
  * @{
  */

/** @brief Open an existing OMRX file for reading
  *
  * This function will open an existing file (or use a supplied `FILE` handle)
  * for reading, and perform an intial scan of its contents.  The file must be
  * seekable.
  *
  * The `omrx` handle supplied should be a fresh handle created with
  * omrx_new().  If it has been previously used, any previous contents will be
  * overwritten.  The file will be scanned for consistency and structure, and
  * an index will be created in memory, but the actual file data is not read
  * until it is actually needed.  It is therefore important not to call
  * omrx_close() until you are sure you have retrieved all the data you need.
  *
  * If the `fp` parameter is `NULL`, then the file specified by `filename` is
  * opened and read.  If `fp` is not `NULL`, then it should be an open `FILE`
  * pointer (opened for reading in binary mode) which will be used instead.  In
  * this case `filename` can still be provided, and will be used in
  * error/warning messages, etc, as the name of the open file.
  *
  * @param[in] omrx     The OMRX instance to use
  * @param[in] filename The name of the file to open
  * @param[in] fp       An open `FILE` pointer to use for file IO, or `NULL`
  *
  * @retval ::OMRX_OK             File opened successfully
  * @retval ::OMRX_ERR_EOF        Unexpected end-of-file encountered
  * @retval ::OMRX_ERR_BAD_MAGIC  Bad data at beginning of file
  * @retval ::OMRX_ERR_BAD_CHUNK  Invalid chunk tag encountered
  * @retval ::OMRX_ERR_BAD_VER    File version is incompatible with library version
  */
omrx_status_t omrx_open(omrx_t omrx, const char *filename, FILE *fp) {
    if (omrx->fp) {
        return omrx_error(omrx, OMRX_ERR_ALREADY_OPEN, "omrx_open() called on already open OMRX handle");
    }
    if (fp) {
        omrx->fp = fp;
        omrx->close_file = false;
    } else {
        omrx->fp = fopen(filename, "rb");
        omrx->close_file = true;
        if (!omrx->fp) {
            return omrx_os_error(omrx, OMRX_ERR_OSERR, "Cannot open '%s' for reading", filename);
        }
    }
    omrx->filename = omrx_strdup(omrx, filename);

    return omrx_scan(omrx);
}

/** @brief Get the version of the OMRX file currently open for reading
  *
  * Returns a binary constant indicating the version of the OMRX standard that
  * the open file was written to.  The upper 16 bits contain the major version,
  * and the lower 16 bits contain the minor version.
  *
  * @param[in] omrx     The OMRX instance to query
  * @param[out] result  Where to store the version information
  *
  * @retval OMRX_OK  Value read successfully and stored in `result`
  */
omrx_status_t omrx_get_version(omrx_t omrx, uint32_t *result) {
    return omrx_get_attr_uint32(omrx->root_chunk, OMRX_ATTR_VER, result);
}

/** @brief Close an OMRX instance opened using omrx_open()
  *
  * Once a file is closed, data which was previously read will still be
  * available, but attempts to access attributes not previously read will fail.
  * It is therefore important if you wish to continue accessing the OMRX
  * instance after closing it that you make sure you have loaded all data you
  * wish to access before calling omrx_close().
  *
  * @param[in] omrx  The OMRX instance to close
  *
  * @retval OMRX_OK          Closed successfully
  * @retval OMRX_WARN_OSERR  Could not close underlying file object
  */
omrx_status_t omrx_close(omrx_t omrx) {
    if (!omrx->fp) {
        return omrx_error(omrx, OMRX_ERR_NOT_OPEN, "omrx_close() called on non-open OMRX handle");
    }
    if (omrx->close_file) {
        if (fclose(omrx->fp)) {
            omrx->fp = NULL;
            return omrx_os_warning(omrx, OMRX_WARN_OSERR, "Close failed");
        }
    }
    omrx->fp = NULL;

    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_write(omrx_t omrx, const char *filename) {
    FILE *fp = fopen(filename, "wb");

    if (!fp) {
        return omrx_os_error(omrx, OMRX_ERR_OSERR, "Cannot open '%s' for writing", filename);
    }
    CHECK_ERR(write_chunk(omrx->root_chunk, fp));

    if (fclose(fp)) {
        return omrx_os_warning(omrx, OMRX_WARN_OSERR, "Close failed");
    }

    return API_RESULT(omrx, OMRX_OK);
}

/** @} */

/** @defgroup chunkapi Chunk-Based API
  *
  * @brief Manipulating chunks and attributes
  *
  * @{
  */

omrx_status_t omrx_get_root_chunk(omrx_t omrx, omrx_chunk_t *result) {
    *result = omrx->root_chunk;
    return OMRX_OK;
}

omrx_status_t omrx_get_chunk_by_id(omrx_t omrx, const char *id, const char *tag, omrx_chunk_t *result) {
    omrx_status_t rc = lookup_chunk_id(omrx, id, result);

    if (rc != OMRX_OK) {
        *result = NULL;
        return API_RESULT(omrx, rc);
    }
    if (tag) {
        // If the caller specified a particular tag, make sure it matches.
        if ((*result)->tagint != TAG_TO_TAGINT(tag)) {
            *result = NULL;
            return API_RESULT(omrx, OMRX_STATUS_NOT_FOUND);
        }
    }

    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_get_child(omrx_chunk_t chunk, const char *tag, omrx_chunk_t *result) {
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
   
    if (tag) {
        uint32_t tagint = TAG_TO_TAGINT(tag);

        chunk = chunk->first_child;
        while (chunk) {
            if (chunk->tagint == tagint) {
                *result = chunk;
                return API_RESULT(omrx, OMRX_OK);
            }
            chunk = chunk->next;
        }
    } else if (chunk->first_child) {
        *result = chunk->first_child;
        return API_RESULT(omrx, OMRX_OK);
    }
    *result = NULL;
    return API_RESULT(omrx, OMRX_STATUS_NOT_FOUND);
}

omrx_status_t omrx_get_next_chunk(omrx_chunk_t chunk, const char *tag, omrx_chunk_t *result) {
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

    omrx_t omrx = chunk->omrx;

    if (tag) {
        uint32_t tagint = chunk->tagint;

        while (chunk->next) {
            chunk = chunk->next;
            if (chunk->tagint == tagint) {
                *result = chunk;
                return API_RESULT(omrx, OMRX_OK);
            }
        }
    } else if (chunk->next) {
        *result = chunk->next;
        return API_RESULT(omrx, OMRX_OK);
    }

    *result = NULL;
    return API_RESULT(omrx, OMRX_STATUS_NOT_FOUND);
}

//FIXME: remove this?
omrx_status_t omrx_get_child_by_id(omrx_chunk_t chunk, const char *tag, const char *id, omrx_chunk_t *result) {
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

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
                return API_RESULT(omrx, OMRX_OK);
            }
        }
        chunk = chunk->next;
    }
    *result = NULL;
    return API_RESULT(omrx, OMRX_STATUS_NOT_FOUND);
}

omrx_status_t omrx_get_parent(omrx_chunk_t chunk, omrx_chunk_t *result) {
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
   
    if (chunk->parent) {
        *result = chunk->parent;
        return API_RESULT(omrx, OMRX_OK);
    }
    *result = NULL;
    return API_RESULT(omrx, OMRX_STATUS_NOT_FOUND);
}

omrx_status_t omrx_add_chunk(omrx_chunk_t chunk, const char *tag, omrx_chunk_t *result) {
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_chunk_t child = new_chunk(omrx, tag);

    CHECK_ALLOC(omrx, child);
    CHECK_ERR(add_child_chunk(chunk, child));
    if (result) {
        *result = child;
    }

    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_del_chunk(omrx_chunk_t chunk) {
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

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

    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_set_attr_str(omrx_chunk_t chunk, uint16_t id, omrx_ownership_t own, char *str) {
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    //FIXME: check for OMRX_ATTR_ID and handle it specially

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        attr = new_attr(chunk, id, OMRX_DTYPE_UTF8, 0, -1);
        CHECK_ALLOC(omrx, attr);
        CHECK_ERR(chunk_add_attr(chunk, attr));
    }
    if (attr->datatype != OMRX_DTYPE_UTF8) {
        return omrx_error(omrx, OMRX_ERR_WRONG_DTYPE, "Attempt to set string value for non-string attribute %s:%04x (type=%04x).", chunk->tag, id, attr->datatype);
    }
    if (attr->data) {
        // FIXME: do we need to worry about people with refs to this?
        omrx->free(omrx, attr->data);
    }
    if (own == OMRX_COPY) {
        attr->data = omrx_strdup(omrx, str);
        CHECK_ALLOC(omrx, attr->data);
    } else {
        attr->data = str;
    }
    attr->size = strlen(attr->data);

    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_get_attr_info(omrx_chunk_t chunk, uint16_t id, struct omrx_attr_info *info) {
    if (!chunk) {
        info->exists = false;
        return OMRX_STATUS_NO_OBJECT;
    }

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        info->exists = false;
        return API_RESULT(omrx, OMRX_STATUS_NOT_FOUND);
    }
    info->exists = true;
    info->encoded_type = attr->datatype;
    info->raw_type = attr->datatype;
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

    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_get_attr_raw(omrx_chunk_t chunk, uint16_t id, size_t *size, void **data) {
    *data = NULL;
    if (size) {
        *size = 0;
    }
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        return API_RESULT(omrx, OMRX_STATUS_NOT_FOUND);
    }
    CHECK_ERR(load_attr_data(attr, data));
    if (size) {
        *size = attr->size;
    }

    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_get_attr_str(omrx_chunk_t chunk, uint16_t id, char **dest) {
    *dest = NULL;
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        return API_RESULT(omrx, OMRX_STATUS_NOT_FOUND);
    }
    if (attr->datatype != OMRX_DTYPE_UTF8) {
        return omrx_error(omrx, OMRX_ERR_WRONG_DTYPE, "Attempt to get string value of non-string attribute %s:%04x (type=%04x).", chunk->tag, id, attr->datatype);
    }
    CHECK_ERR(load_attr_data(attr, (void **)dest));

    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_set_attr_uint32(omrx_chunk_t chunk, uint16_t id, uint32_t value) {
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        attr = new_attr(chunk, id, OMRX_DTYPE_U32, 4, -1);
        CHECK_ALLOC(omrx, attr);
        CHECK_ERR(chunk_add_attr(chunk, attr));
    }
    if (attr->datatype != OMRX_DTYPE_U32) {
        return omrx_error(omrx, OMRX_ERR_WRONG_DTYPE, "Attempt to set uint32 value for non-uint32 attribute %s:%04x (type=%04x).", chunk->tag, id, attr->datatype);
    }
    if (!attr->data) {
        attr->data = omrx->alloc(omrx, 4);
        CHECK_ALLOC(omrx, attr->data);
    }
    *((uint32_t *)attr->data) = value;

    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_get_attr_uint32(omrx_chunk_t chunk, uint16_t id, uint32_t *dest) {
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        *dest = 0;
        return API_RESULT(omrx, OMRX_STATUS_NOT_FOUND);
    }
    if (attr->datatype != OMRX_DTYPE_U32) {
        return omrx_error(omrx, OMRX_ERR_WRONG_DTYPE, "Attempt to get uint32 value of non-uint32 attribute %s:%04x (type=%04x).", chunk->tag, id, attr->datatype);
    }
    if (!attr->data) {
        CHECK_ERR(load_attr_data(attr, &attr->data));
    }
    *dest = *((uint32_t *)attr->data);

    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_set_attr_float32_array(omrx_chunk_t chunk, uint16_t id, omrx_ownership_t own, uint16_t cols, uint32_t rows, float *data) {
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        attr = new_attr(chunk, id, OMRX_DTYPE_F32_ARRAY, 0, -1);
        CHECK_ALLOC(omrx, attr);
        CHECK_ERR(chunk_add_attr(chunk, attr));
    }
    if (attr->datatype != OMRX_DTYPE_F32_ARRAY) {
        return omrx_error(omrx, OMRX_ERR_WRONG_DTYPE, "Attempt to set float-array value for non-float-array attribute %s:%04x (type=%04x).", chunk->tag, id, attr->datatype);
    }
    if (attr->data) {
        omrx->free(omrx, attr->data);
    }
    attr->size = 4 * rows * cols;
    attr->cols = cols;
    if (own == OMRX_COPY) {
        attr->data = omrx->alloc(omrx, attr->size);
        CHECK_ALLOC(omrx, attr->data);
        memcpy(attr->data, data, attr->size);
    } else {
        attr->data = data;
    }

    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_get_attr_float32_array(omrx_chunk_t chunk, uint16_t id, uint16_t *cols, uint32_t *rows, float **data) {
    *data = NULL;
    if (cols) {
        *cols = 0;
    }
    if (rows) {
        *rows = 0;
    }
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t attr = NULL;

    CHECK_ERR(find_attr(chunk, id, &attr));
    if (!attr) {
        return API_RESULT(omrx, OMRX_STATUS_NOT_FOUND);
    }
    if (attr->datatype != OMRX_DTYPE_F32_ARRAY) {
        return omrx_error(omrx, OMRX_ERR_WRONG_DTYPE, "Attempt to get float32-array value of non-float32-array attribute %s:%04x (type=%04x).", chunk->tag, id, attr->datatype);
    }
    CHECK_ERR(load_attr_data(attr, (void **)data));
    if (cols) {
        *cols = attr->cols;
    }
    if (rows) {
        *rows = (attr->size / attr->cols) / 4;
    }

    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_release_attr_data(omrx_chunk_t chunk, uint16_t id) {
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    //FIXME: implement this
    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_del_attr(omrx_chunk_t chunk, uint16_t id) {
    if (!chunk) return OMRX_STATUS_NO_OBJECT;

    omrx_t omrx = chunk->omrx;
    omrx_attr_t prev = (omrx_attr_t)(&chunk->attrs);
    omrx_attr_t attr;

    while (prev->next) {
        if (prev->next->id == id) {
            attr = prev->next;
            prev->next = attr->next;
            CHECK_ERR(free_attr(attr));
            return API_RESULT(omrx, OMRX_OK);
        }
        prev = prev->next;
    }

    return API_RESULT(omrx, OMRX_STATUS_NOT_FOUND);
}

/** @} */

/** @} */

