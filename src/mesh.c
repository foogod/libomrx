#include "omrx.h"
#include "omrx_internal.h"

typedef struct {
    int vdata_count;
    double *vdata;
    int prim_count;
    omrx_primitive_t *prim;
} omrx_mesh_t;

static omrx_status_t omrx_get_mesh_from_chunk(omrx_chunk_t chunk, omrx_mesh_t **dest);

omrx_status_t omrx_get_mesh_by_id(omrx_t omrx, const char *id, omrx_mesh_t **dest) {
    omrx_chunk_t chunk;

    *dest = NULL;
    OMRX_CHECK_OK(omrx_get_chunk_by_id(omrx, id, "meSH", &chunk));
    return get_mesh_from_chunk(chunk, dest);
}

omrs_status_t omrx_get_mesh_by_number(omrx_t omrx, int number, omrx_mesh_t **dest) {
    omrx_chunk_t chunk;
    int i;

    *dest = NULL;
    OMRX_CHECK_ERR(omrx_get_root_chunk(omrx, &chunk));
    OMRX_CHECK_ERR(omrx_get_child(chunk, "meSH", &chunk));
    for (i = 0; i < number; i++) {
        OMRX_CHECK_OK(omrx_get_next_chunk(chunk, "meSH", &chunk));
    }
    return get_mesh_from_chunk(chunk, dest);
}

static omrx_status_t omrx_get_mesh_from_chunk(omrx_chunk_t chunk, omrx_mesh_t **dest) {
    return OMRX_OK; //FIXME
}

