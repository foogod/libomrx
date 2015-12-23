#include <stdio.h>
#include <stdlib.h>

#include "omrx.h"

#define CHECK_OMRX_ERR(x) if ((x) < 0) { fprintf(stderr, "Unexpected error from libomrx.  Exiting.\n"); exit(1); }

int main(int argc, char *argv[]) {
    struct omrx *omrx;
    struct omrx_chunk *chunk;
    float *point_data;
    uint16_t cols;
    uint32_t rows;
    unsigned int i, j;
    char *filename;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return 1;
    }

    filename = argv[1];

    // Start of libomrx-related code

    if (omrx_init() != OMRX_OK) {
        fprintf(stderr, "omrx_init failed!\n");
        return 1;
    }

    CHECK_OMRX_ERR(omrx_new(NULL, &omrx));
    CHECK_OMRX_ERR(omrx_open(omrx, filename, NULL));

    // Find the mESH chunk with id="test" and get its first VRTx child
    CHECK_OMRX_ERR(omrx_get_chunk_by_id(omrx, "test", "mESH", &chunk));
    CHECK_OMRX_ERR(omrx_get_child(chunk, "VRTx", &chunk));

    // Get the data from the 'data' attribute
    CHECK_OMRX_ERR(omrx_get_attr_float32_array(chunk, OMRX_ATTR_DATA, &cols, &rows, &point_data));

    // Print it out
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            printf("%f ", point_data[i * cols + j]);
        }
        printf("\n");
    }

    CHECK_OMRX_ERR(omrx_free(omrx));

    return 0;
}
