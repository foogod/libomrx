#include <omrx.h>

typedef enum {
    OMRX_RIGHT_HANDED = 1,
    OMRX_LEFT_HANDED = -1,
} omrx_chiral_t;

typedef enum {
    OMRX_X_UP = 1,
    OMRX_Y_UP = 2,
    OMRX_Z_UP = 3,
    OMRX_X_DOWN = -1,
    OMRX_Y_DOWN = -2,
    OMRX_Z_DOWN = -3,
} omrx_upaxis_t;

typedef double[4][4] omrx_mat4_t;

omrx_mat4_t *omrx_identity_matrix(void) {
    omrx_mat4_t *mat = malloc(sizeof(omrx_mat4_t));

    //FIXME: check alloc
    memset(mat, sizeof(*mat), 0);
    mat[0][0] = 1;
    mat[1][1] = 1;
    mat[2][2] = 1;
    mat[3][3] = 1;
    return mat;
}

omrx_status_t omrx_context_set_transform_mnemonic(omrx_context_t context, omrx_chiral_t chirality, omrx_upaxis_t orientation) {
    omrx_mat4_t mat;
    int vert_axis, vert_sign, horiz_axis, depth_axis;

    vert_axis = abs(orientation);
    vert_sign = orientation / vert_axis;
    vert_axis--;
    horiz_axis = (vert_axis == 0) ? 1 : 0;
    depth_axis = (vert_axis == 2) ? 1 : 2;

    memset(mat, sizeof(mat), 0);
    mat[vert_axis][2] = vert_sign;
    mat[horiz_axis][0] = 1;
    mat[depth_axis][1] = vert_sign * chirality;
    mat[3][3] = 1;

    return omrx_context_set_transform_matrix(context, mat);
}

omrx_status_t omrx_context_set_transform_matrix(omrx_context_t context, const omrx_mat4_t *mat) {
    memcpy(&context->transform, mat, sizeof(omrx_mat4_t));

    return OMRX_OK;
}
