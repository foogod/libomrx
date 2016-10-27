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

omrx_status_t omrx_chunk_to_model(omrx_chunk_t chunk, omrx_model_t *result) {
    omrx_t omrx;

    *result = NULL;
    if (!chunk) return OMRX_STATUS_NO_OBJECT;
    omrx = chunk->omrx;

    if (chunk->tagint != TAG_TO_TAGINT("MoDL")) {
        return omrx_error(omrx, OMRX_ERR_WRONG_CHUNK, "Attempt to convert %s chunk to model", chunk->tag);
    }
    *result = (omrx_model_t)chunk;
    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_model_to_chunk(omrx_model_t model, omrx_chunk_t *result) {
    omrx_t omrx;

    *result = NULL;
    if (!model) return OMRX_STATUS_NO_OBJECT;

    *result = (omrx_chunk_t)model;
    omrx = (*result)->omrx;
    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_find_model_by_id(omrx_t omrx, const char *id, omrx_model_t *result) {
    omrx_chunk_t chunk;

    *result = NULL;
    CHECK_OK(omrx_get_chunk_by_id(omrx, id, "MoDL", &chunk));
    return omrx_chunk_to_model(chunk, result);
}

omrx_status_t omrx_find_model_by_index(omrx_t omrx, int index, omrx_model_t *result) {
    omrx_chunk_t chunk;
    int i;

    *result = NULL;
    CHECK_OK(omrx_get_root_chunk(omrx, &chunk));
    CHECK_OK(omrx_get_child(chunk, "MoDL", index, &chunk));
    return omrx_chunk_to_model(chunk, result);
}

omrx_status_t omrx_model_get_id(omrx_model_t model, char **result) {
    omrx_chunk_t chunk;

    *result = NULL;
    CHECK_OK(omrx_model_to_chunk(model, &chunk));
    CHECK_OK(omrx_get_attr_str(chunk, OMRX_ATTR_ID, result));
    return API_RESULT(chunk->omrx, OMRX_OK);
}

omrx_status_t omrx_model_get_name(omrx_model_t model, char **result) {
    omrx_chunk_t chunk;

    *result = NULL;
    CHECK_OK(omrx_model_to_chunk(model, &chunk));
    CHECK_OK(omrx_get_attr_str(chunk, OMRX_ATTR_NAME, result));
    return API_RESULT(chunk->omrx, OMRX_OK);
}

omrx_status_t omrx_model_get_lod_count(omrx_model_t model, int *result) {
    omrx_chunk_t model_chunk, lod_chunk;

    *result = 0;
    CHECK_OK(omrx_model_to_chunk(model, &model_chunk));
    CHECK_OK(omrx_get_child(model_chunk, "MLOd", 0, &lod_chunk));
    while (lod_chunk) {
        (*result)++;
        CHECK_ERR(omrx_get_next_chunk(lod_chunk, "MLOd", &lod_chunk));
    }
    return API_RESULT(model_chunk->omrx, OMRX_OK);
}

omrx_status_t omrx_new_model(omrx_t omrx, int index, omrx_model_t *result) {
    omrx_chunk_t model_chunk;

    *result = NULL;
    CHECK_OK(omrx_add_chunk(omrx, index, "MoDL", &model_chunk));
    return omrx_chunk_to_model(model_chunk, result);
}

omrx_status_t omrx_model_set_id(omrx_model_t model, const char *id) {
    omrx_chunk_t chunk;

    CHECK_OK(omrx_model_to_chunk(model, &chunk));
    CHECK_OK(omrx_set_attr_str(chunk, OMRX_ATTR_ID, id));
    return API_RESULT(chunk->omrx, OMRX_OK);
}

omrx_status_t omrx_model_set_name(omrx_model_t model, const char *name) {
    omrx_chunk_t chunk;

    CHECK_OK(omrx_model_to_chunk(model, &chunk));
    CHECK_OK(omrx_set_attr_str(chunk, OMRX_ATTR_NAME, name));
    return API_RESULT(chunk->omrx, OMRX_OK);
}

omrx_status_t omrx_model_free(omrx_model_t model) {
    omrx_chunk_t chunk;

    CHECK_OK(omrx_model_to_chunk(model, &chunk));
    /* Nothing to do in this case */
    return API_RESULT(chunk->omrx, OMRX_OK);
}

omrx_status_t omrx_model_get_matcount(omrx_model_t model, unsigned *result);
omrx_status_t omrx_model_get_material(omrx_model_t model, unsigned index, omrx_material_t *result);

omrx_status_t omrx_model_lod_by_index(omrx_model_t model, int index, omrx_lod_t *result) {
    omrx_chunk_t model_chunk, lod_chunk;

    *result = NULL;
    CHECK_OK(omrx_model_to_chunk(model, &model_chunk));
    CHECK_OK(omrx_get_child(model_chunk, "MLOd", index, &lod_chunk));
    CHECK_OK(omrx_chunk_to_lod(lod_chunk, result));
    return API_RESULT(model_chunk->omrx, OMRX_OK);
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

static omrx_status_t find_lod(omrx_chunk_t model_chunk, float32_t ppsu, omrx_chunk_t *result) {
    omrx_chunk_t tmpchunk;
    float32_t model_scale, ppsu, lod_ppsu;

    *result = NULL;

    // LOD chunks are required to be listed in highest-to-lowest PPSU order.
    // Iterate through all of them until we find the least-detailed one that is
    // still greater than or equal to the requested PPSU.
    CHECK_OK(omrx_get_child(model_chunk, "MLOd", 0, &tmpchunk));
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
        *result = tmpchunk;
    }
    return OMRX_OK;
}

//FIXME: This should take ppsu in context-units instead of ppsm
omrx_status_t omrx_model_lod_by_ppsm(omrx_model_t model, float32_t ppsm, omrx_lod_t *result) {
    omrx_t omrx;
    omrx_chunk_t model_chunk, lod_chunk;
    float32 ppsu;

    *result = NULL;
    CHECK_OK(omrx_model_to_chunk(model, &model_chunk));
    omrx = model_chunk->omrx;
    ppsu = omrx_ppsm_to_ppsu(get_model_scale(model_chunk), ppsm);
    CHECK_OK(find_lod(model_chunk, ppsu, &lod_chunk));
    return omrx_chunk_to_lod(lod_chunk, result);
}

//////////////// lod routines /////////////////

omrx_status_t omrx_chunk_to_lod(omrx_chunk_t chunk, omrx_lod_t *result) {
    omrx_t omrx;

    *result = NULL;
    if (!chunk) return OMRX_STATUS_NO_OBJECT;
    omrx = chunk->omrx;

    if (chunk->tagint != TAG_TO_TAGINT("MLOd")) {
        return omrx_error(omrx, OMRX_ERR_WRONG_CHUNK, "Attempt to convert %s chunk to lod", chunk->tag);
    }
    *result = (omrx_lod_t)chunk;
    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_lod_to_chunk(omrx_lod_t lod, omrx_chunk_t *result) {
    omrx_t omrx;

    *result = NULL;
    if (!lod) return OMRX_STATUS_NO_OBJECT;

    *result = (omrx_chunk_t)lod;
    omrx = (*result)->omrx;
    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_lod_free(omrx_lod_t lod) {
    omrx_chunk_t chunk;

    CHECK_OK(omrx_lod_to_chunk(lod, &chunk));
    /* Nothing to do in this case */
    return API_RESULT(chunk->omrx, OMRX_OK);
}

omrx_status_t omrx_lod_get_ppsu(omrx_lod_t lod, float32_t *result) {
    omrx_chunk_t lod_chunk;
    int i;

    *result = 0;
    CHECK_OK(omrx_lod_to_chunk(lod, &lod_chunk));
    //FIXME: need to convert to context-units
    *result = get_lod_ppsu(lod_chunk);
    return API_RESULT(lod_chunk->omrx, OMRX_OK);
}

static omrx_status_t omrx_lod_get_mesh(omrx_lod_t lod, omrx_mesh_t *result) {
    omrx_chunk_t lod_chunk, mesh_chunk;
    char *mesh_id;

    CHECK_OK(omrx_lod_to_chunk(lod, &lod_chunk));
    CHECK_OK(omrx_get_attr_str(lod_chunk, OMRX_ATTR_ID, &mesh_id));
    CHECK_OK(omrx_get_chunk_by_id(lod_chunk->omrx, mesh_id, "MesH", &mesh_chunk));
    return omrx_chunk_to_mesh(mesh_chunk, result);
}

//TODO: convert data to context space when reading
omrx_status_t omrx_lod_get_vdata(omrx_lod_t lod, omrx_meshdata_type_t type, unsigned index, struct omrx_meshdata *result) {
    omrx_mesh_t mesh;
    omrx_status_t status;

    CHECK_OK(omrx_lod_get_mesh(lod, &mesh));
    status = omrx_mesh_get_vdata(mesh, type, index, result);
    omrx_mesh_free(mesh);
    return status;
}

omrx_status_t omrx_lod_get_polys(omrx_lod_t lod, struct omrx_polys *result) {
    omrx_mesh_t mesh;
    omrx_status_t status;

    CHECK_OK(omrx_lod_get_mesh(lod, &mesh));
    status = omrx_mesh_get_polys(mesh, result);
    omrx_mesh_free(mesh);
    return status;
}

////////////////// mesh routines /////////////////

omrx_status_t omrx_chunk_to_mesh(omrx_chunk_t chunk, omrx_mesh_t *result) {
    omrx_t omrx;

    *result = NULL;
    if (!chunk) return OMRX_STATUS_NO_OBJECT;
    omrx = chunk->omrx;

    if (chunk->tagint != TAG_TO_TAGINT("MesH")) {
        return omrx_error(omrx, OMRX_ERR_WRONG_CHUNK, "Attempt to convert %s chunk to mesh", chunk->tag);
    }
    *result = (omrx_mesh_t)chunk;
    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_mesh_to_chunk(omrx_mesh_t mesh, omrx_chunk_t *result) {
    omrx_t omrx;

    *result = NULL;
    if (!mesh) return OMRX_STATUS_NO_OBJECT;

    *result = (omrx_chunk_t)mesh;
    omrx = (*result)->omrx;
    return API_RESULT(omrx, OMRX_OK);
}

omrx_status_t omrx_mesh_free(omrx_mesh_t mesh) {
    omrx_chunk_t chunk;

    CHECK_OK(omrx_mesh_to_chunk(mesh, &chunk));
    /* Nothing to do in this case */
    return API_RESULT(chunk->omrx, OMRX_OK);
}

static omrx_status_t find_vdat_chunk(omrx_chunk_t mesh_chunk, omrx_meshdata_type_t type, int index, omrx_chunk_t *result) {
    omrx_status_t status;
    uint32_t value;

    CHECK_OK(omrx_get_child(mesh_chunk, "VDat", 0, result));
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

omrx_status_t omrx_mesh_get_vdata(omrx_mesh_t mesh, omrx_meshdata_type_t type, unsigned index, struct omrx_meshdata *result) {
    omrx_chunk_t mesh_chunk, vdat_chunk;
    struct omrx_attr_info attr_info;

    result->data = NULL;
    result->datatype = OMRX_DTYPE_INVALID;
    result->rows = 0;
    result->cols = 0;

    CHECK_OK(omrx_mesh_to_chunk(mesh, &mesh_chunk));
    CHECK_OK(find_vdat_chunk(mesh_chunk, type, index, &vdat_chunk));
    CHECK_OK(omrx_get_attr_info(vdat_chunk, ATTR_DATA, &attr_info));
    CHECK_OK(omrx_get_attr_raw(vdat_chunk, ATTR_DATA, NULL, &result->data));
    result->datatype = attr_info.raw_type;
    result->rows = attr_info.rows;
    result->cols = attr_info.cols;

    return API_RESULT(mesh_chunk->omrx, OMRX_OK);
}

omrx_status_t omrx_mesh_get_polys(omrx_mesh_t mesh, struct omrx_polys *result) {
    omrx_chunk_t mesh_chunk, poly_chunk;
    struct omrx_attr_info attr_info;

    result->polytype = OMRX_POLY_INVALID;
    result->data = NULL;
    result->datatype = OMRX_DTYPE_INVALID;
    result->count = 0;

    CHECK_OK(omrx_mesh_to_chunk(mesh, &mesh_chunk));
    CHECK_OK(omrx_get_child(mesh_chunk, "PoLy", 0, &poly_chunk));

    CHECK_OK(omrx_get_attr_info(poly_chunk, ATTR_DATA, &attr_info));
    CHECK_OK(omrx_get_attr_raw(poly_chunk, ATTR_DATA, NULL, &result->data));
    result->datatype = attr_info.raw_type;
    //FIXME: check that cols matches poly type?
    result->count = attr_info.rows * attr_info.cols;

    return API_RESULT(mesh_chunk->omrx, OMRX_OK);
}

omrx_status_t omrx_mesh_set_polys(omrx_mesh_t mesh, struct omrx_polys *polys) {
    omrx_chunk_t mesh_chunk, poly_chunk;
    struct omrx_attr_info attr_info;

    result->polytype = OMRX_POLY_INVALID;
    result->data = NULL;
    result->datatype = OMRX_DTYPE_INVALID;
    result->count = 0;

    CHECK_OK(omrx_mesh_to_chunk(mesh, &mesh_chunk));
    CHECK_OK(omrx_get_child(mesh_chunk, "PoLy", 0, &poly_chunk));

    CHECK_OK(omrx_get_attr_info(poly_chunk, ATTR_DATA, &attr_info));
    CHECK_OK(omrx_get_attr_raw(poly_chunk, ATTR_DATA, NULL, &result->data));
    result->datatype = attr_info.raw_type;
    //FIXME: check that cols matches poly type?
    result->count = attr_info.rows * attr_info.cols;

    return API_RESULT(mesh_chunk->omrx, OMRX_OK);
}

