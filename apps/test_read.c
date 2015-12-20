#include <stdio.h>
#include <stdlib.h>

#include "omrx.h"

#define CHECK_OMRX_ERR(x) if ((x) < 0) { fprintf(stderr, "Unexpected error from libomrx.  Exiting.\n"); exit(1); }

int main(int argc, char *argv[]) {
    struct omrx *omrx;
    struct omrx_chunk *chunk;
    float *point_data;
    unsigned int i, j;
    char *filename;
    struct omrx_attr_info info;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return 1;
    }

    filename = argv[1];

    // Start of libomrx-related code

    if (!omrx_init()) {
        fprintf(stderr, "omrx_init failed!\n");
        return 1;
    }

    omrx = omrx_new();
    if (!omrx) exit(1);

    CHECK_OMRX_ERR(omrx_open(omrx, filename));

    // Find the toplevel mESH chunk with id="test" and get its first VRTx child
    chunk = omrx_get_root_chunk(omrx);
    CHECK_OMRX_ERR(omrx_get_child_by_id(chunk, "mESH", "test", &chunk));
    CHECK_OMRX_ERR(omrx_get_child(chunk, "VRTx", &chunk));
    // Get info for the 'data' attribute
    CHECK_OMRX_ERR(omrx_get_attr_info(chunk, OMRX_ATTR_DATA, &info));
    if (omrx_last_result(omrx) != OMRX_OK) {
        fprintf(stderr, "Could not find vertex data for 'test' mesh\n");
        return 1;
    }

    if (!info.is_array || info.elem_type != OMRX_DTYPE_F32) {
        fprintf(stderr, "VRTx data is wrong type (%04x)\n", info.encoded_type);
        return 1;
    }
    CHECK_OMRX_ERR(omrx_get_attr_float32_array(chunk, OMRX_ATTR_DATA, &point_data));
    for (i = 0; i < info.rows; i++) {
        for (j = 0; j < info.cols; j++) {
            printf("%f ", point_data[i * info.cols + j]);
        }
        printf("\n");
    }

    CHECK_OMRX_ERR(omrx_close(omrx));
    CHECK_OMRX_ERR(omrx_free(omrx));

    return 0;
}
