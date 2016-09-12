#include "omrx.h"
#include "omrx_internal.h"

struct omrx_meshdata {
    uint32_t vertices_len;
    float32_t *vertices;
    float32_t *normals;
    int32_t *mat_indices;
    int32_t *texcoords;
    omrx_polytype_t poly_type;
    omrx_chiral_t poly_face;
    uint32_t poly_data_len;
    int32_t *poly_data;
};

#define OMRX_MESHDATA_VERTS       0x01
#define OMRX_MESHDATA_NORMALS     0x02
#define OMRX_MESHDATA_MATIDX      0x03
#define OMRX_MESHDATA_MATCOORDS   0x04
#define OMRX_MESHDATA_POLYS       0x05

omrx_status_t omrx_model_from_chunk(omrx_chunk_t chunk, omrx_model_t *dest) {
    omrx_t omrx;

    *dest = NULL;
    if (!chunk) {
        return OMRX_STATUS_NO_OBJECT;
    }
    omrx = chunk->omrx;

    if (chunk->tagint != TAG_TO_TAGINT("MoDL")) {
        return omrx_error(omrx, OMRX_ERR_WRONG_CHUNK, "Attempt to convert %s chunk to model", chunk->tag);
    }
    *dest = (omrx_model_t)chunk;
    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_chunk_from_model(omrx_model_t model, omrx_chunk_t *dest) {
    omrx_t omrx;

    *dest = NULL;
    if (!model) {
        return OMRX_STATUS_NO_OBJECT;
    }

    *dest = (omrx_chunk_t)model;
    omrx = (*dest)->omrx;
    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_find_model_by_id(omrx_t omrx, const char *id, omrx_model_t *dest) {
    omrx_chunk_t chunk;

    *dest = NULL;
    CHECK_OK(omrx_get_chunk_by_id(omrx, id, "MoDL", &chunk));
    return omrx_model_from_chunk(chunk, dest);
}

omrs_status_t omrx_find_model_by_index(omrx_t omrx, int index, omrx_model_t **dest) {
    omrx_chunk_t chunk;
    int i;

    *dest = NULL;
    CHECK_OK(omrx_get_root_chunk(omrx, &chunk));
    CHECK_OK(omrx_get_child(chunk, "MoDL", &chunk));
    for (i = 0; i < index; i++) {
        CHECK_OK(omrx_get_next_chunk(chunk, "MoDL", &chunk));
    }
    return omrx_model_from_chunk(chunk, dest);
}

omrx_status_t omrx_get_model_id(omrx_model_t model, char **dest) {
    omrx_chunk_t chunk;

    *dest = NULL;
    CHECK_OK(omrx_chunk_from_model(model, &chunk));
    CHECK_OK(omrx_get_attr_str(chunk, OMRX_ATTR_ID, dest));
    return API_RESULT(chunk->omrx, OMRX_OK);
}

omrx_status_t omrx_get_model_name(omrx_model_t model, char **dest) {
    omrx_chunk_t chunk;

    *dest = NULL;
    CHECK_OK(omrx_chunk_from_model(model, &chunk));
    CHECK_OK(omrx_get_attr_str(chunk, OMRX_ATTR_NAME, dest));
    return API_RESULT(chunk->omrx, OMRX_OK);
}

omrx_status_t omrx_get_model_mesh_count(omrx_model_t model, int *dest) {
    omrx_chunk_t model_chunk, lod_chunk;

    *dest = 0;
    CHECK_OK(omrx_chunk_from_model(model, &model_chunk));
    CHECK_OK(omrx_get_child(model_chunk, "MLOd", &lod_chunk));
    while (lod_chunk) {
        (*dest)++;
        CHECK_ERR(omrx_get_next_chunk(lod_chunk, "MLOd", &lod_chunk));
    }
    return API_RESULT(model_chunk->omrx, OMRX_OK);
}

omrx_status_t omrx_add_model(omrx_t omrx, int index, omrx_model_t *dest) {
    omrx_chunk_t model_chunk;

    *dest = NULL;
    CHECK_OK(omrx_add_chunk(omrx, index, "MoDL", &model_chunk));
    return omrx_model_from_chunk(model_chunk, dest);
}

omrx_status_t omrx_set_model_id(omrx_model_t model, const char *id) {
    omrx_chunk_t chunk;

    CHECK_OK(omrx_chunk_from_model(model, &chunk));
    CHECK_OK(omrx_set_attr_str(chunk, OMRX_ATTR_ID, id));
    return API_RESULT(chunk->omrx, OMRX_OK);
}

omrx_status_t omrx_set_model_name(omrx_model_t model, const char *name) {
    omrx_chunk_t chunk;

    CHECK_OK(omrx_chunk_from_model(model, &chunk));
    CHECK_OK(omrx_set_attr_str(chunk, OMRX_ATTR_NAME, name));
    return API_RESULT(chunk->omrx, OMRX_OK);
}


//////////////////

omrx_status_t omrx_mesh_from_chunk(omrx_chunk_t chunk, omrx_mesh_t *dest) {
    omrx_t omrx;

    *dest = NULL;
    if (!chunk) {
        return OMRX_STATUS_NO_OBJECT;
    }
    omrx = chunk->omrx;

    if (chunk->tagint != TAG_TO_TAGINT("Mesh")) {
        return omrx_error(omrx, OMRX_ERR_WRONG_CHUNK, "Attempt to convert %s chunk to mesh", chunk->tag);
    }
    *dest = (omrx_mesh_t)chunk;
    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_chunk_from_mesh(omrx_mesh_t mesh, omrx_chunk_t *dest) {
    omrx_t omrx;

    *dest = NULL;
    if (!mesh) {
        return OMRX_STATUS_NO_OBJECT;
    }

    *dest = (omrx_chunk_t)mesh;
    omrx = (*dest)->omrx;
    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_find_model_mesh_by_index(omrx_model_t model, int index, omrx_mesh_t *dest_mesh, float32_t *dest_ppsu) {
}

//FIXME: turn this into a generic get_num_chunk?
static omrx_status_t get_lod_chunk(omrx_chunk_t model_chunk, unsigned index, omrx_chunk_t *dest) {
    int i;

    CHECK_OK(omrx_get_child(model_chunk, "MLOd", dest));
    for (i = 0; i < index; i++) {
        CHECK_OK(omrx_get_next_chunk(*dest, "MLOd", dest));
    }
    return OMRX_OK;
}

static float32_t get_model_scale(omrx_chunk_t chunk) {
    float32_t value;
    omrx_status_t status;

    status = omrx_get_attr_float32(chunk, OMRX_ATTR_SCALE, &value);
    if (status != OMRX_STATUS_OK) {
        omrx_warning(chunk->omrx, OMRX_WARN_BAD_ATTR, "Model has bad or missing 'scale' attribute.  Result may be wrong size.");
        value = 1.0;
    }
    return value;
}

static float32_t get_lod_ppsu(omrx_chunk_t chunk) {
    float32_t value;
    omrx_status_t status;

    status = omrx_get_attr_float32(chunk, OMRX_ATTR_PPSU, &value);
    if (status != OMRX_STATUS_OK) {
        value = 0;
    }
    if (value < 0) value = 0;
    return value;
}

static omrx_status_t find_lod(omrx_chunk_t model_chunk, float32_t ppsu, omrx_chunk_t *dest) {
    omrx_chunk_t tmpchunk;
    float32_t model_scale, ppsu, lod_ppsu;

    *dest = NULL;

    // LOD chunks are required to be listed in highest-to-lowest PPSU order.
    // Iterate through all of them until we find the least-detailed one that is
    // still greater than or equal to the requested PPSU.
    CHECK_OK(omrx_get_child(model_chunk, "MLOd", &tmpchunk));
    while (1) {
        CHECK_ERR(omrx_get_next_chunk(tmpchunk, "MLOd", &tmpchunk));
        if (!tmpchunk) {
            // We've hit the end.  Return the last one we found.
            break;
        }
        lod_ppsu = get_lod_ppsu(tmpchunk);
        if (lod_ppsu <= 0) {
            omrx_warning(chunk->omrx, OMRX_WARN_BAD_ATTR, "Model LOD entry has bad or missing 'ppsu' attribute.  Ignoring LOD entry.");
            continue;
        }
        if (lod_ppsu < ppsu) {
            // This LOD chunk (and all those that follow) has lower PPSU than
            // we're looking for.  Return the last acceptable one we found.
            break;
        }
        // This LOD chunk is acceptable, but we should keep looking to see if
        // any of the following ones are a better match.
        *dest = tmpchunk;
    }
    return OMRX_OK;
}

//FIXME: This should take ppsu in context-units instead of ppsm
omrx_status_t omrx_find_model_mesh_by_lod(omrx_model_t model, float32_t ppsm, omrx_mesh_t *dest) {
    omrx_t omrx;
    omrx_chunk_t model_chunk, lod_chunk, mesh_chunk;
    float32 ppsu;
    char *mesh_id;

    *dest = NULL;
    CHECK_OK(omrx_chunk_from_model(model, &model_chunk));
    omrx = model_chunk->omrx;
    ppsu = omrx_ppsm_to_ppsu(get_model_scale(model_chunk), ppsm);
    CHECK_OK(find_lod(model_chunk, ppsu, &lod_chunk));
    CHECK_OK(omrx_get_attr_str(lod_chunk, OMRX_ATTR_ID, &mesh_id));
    CHECK_OK(omrx_get_chunk_by_id(omrx, mesh_id, "Mesh", &mesh_chunk));
    return omrx_mesh_from_chunk(mesh_chunk, dest);
}

omrx_status_t omrx_get_model_mesh_ppsu(omrx_model_t model, unsigned index, float32_t *dest) {
    omrx_chunk_t model_chunk, lod_chunk;
    int i;

    *dest = 0;
    CHECK_OK(omrx_chunk_from_model(model, &model_chunk));
    CHECK_OK(get_lod_chunk(model_chunk, index, &lod_chunk));
    //FIXME: need to convert to context-units
    *dest = get_lod_ppsu(lod_chunk);
    return API_RESULT(model_chunk->omrx, OMRX_OK);
}

//TODO: convert data to context space when reading
omrx_status_t omrx_get_mesh_vdata(omrx_mesh_t mesh, omrx_meshdata_type_t type, unsigned index, struct omrx_meshdata *dest) {
    omrx_t omrx;
    omrx_chunk_t mesh_chunk, vdat_chunk;
    struct omrx_attr_info attr_info;

    dest->data = NULL;
    dest->datatype = OMRX_DTYPE_INVALID;
    dest->rows = 0;
    dest->cols = 0;

    CHECK_OK(omrx_chunk_from_mesh(mesh, &mesh_chunk));
    omrx = mesh_chunk->omrx;
    CHECK_OK(find_vdat_chunk(mesh_chunk, type, index, &vdat_chunk));
    CHECK_OK(omrx_get_attr_info(vdat_chunk, ATTR_DATA, &attr_info));
    CHECK_OK(omrx_get_attr_raw(vdat_chunk, ATTR_DATA, NULL, &dest->data));
    dest->datatype = attr_info.raw_type;
    dest->rows = attr_info.rows;
    dest->cols = attr_info.cols;

    return API_RESULT(omrx, OMRX_OK);
}

static omrx_status_t find_vdat_chunk(omrx_chunk_t mesh_chunk, omrx_meshdata_type_t type, int index, omrx_chunk_t *result) {
    omrx_status_t status;
    uint32_t value;

    CHECK_OK(omrx_get_child(mesh_chunk, "VDat", result));
    while (1) {
        do {
            //FIXME: errors here should be warnings
            status = omrx_get_attr_uint32(*result, OMRX_ATTR_TYPE, &value);
            if (status != OMRX_OK) break;
            if (value != type) break;
            status = omrx_get_attr_uint32(*result, OMRX_ATTR_INDEX, &value);
            if (status != OMRX_OK) break;
            if (value != index) break;
            return OMRX_OK;
        } while (0);
        CHECK_OK(omrx_get_next_chunk(*result, "VDat", result));
    }
}

omrx_status_t omrx_get_mesh_polys(omrx_mesh_t mesh, struct omrx_polys *dest) {
    omrx_t omrx;
    omrx_chunk_t mesh_chunk, poly_chunk;
    struct omrx_attr_info attr_info;

    dest->polytype = OMRX_POLY_INVALID;
    dest->data = NULL;
    dest->datatype = OMRX_DTYPE_INVALID;
    dest->count = 0;

    CHECK_OK(omrx_chunk_from_mesh(mesh, &mesh_chunk));
    omrx = mesh_chunk->omrx;
    CHECK_OK(omrx_get_child(mesh_chunk, "PoLy", &poly_chunk));

    CHECK_OK(omrx_get_attr_info(poly_chunk, ATTR_DATA, &attr_info));
    CHECK_OK(omrx_get_attr_raw(poly_chunk, ATTR_DATA, NULL, &dest->data));
    dest->datatype = attr_info.raw_type;
    //FIXME: check that cols matches poly type?
    dest->count = attr_info.rows * attr_info.cols;

    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_get_model_matcount(omrx_model_t model, unsigned *dest);
omrx_status_t omrx_get_model_material(omrx_model_t model, unsigned index, omrx_material_t *dest);

