#include <stdio.h>
#include <stdlib.h>

#include "omrx.h"

#define CHECK_OMRX_ERR(x) if ((x) < 0) { fprintf(stderr, "Unexpected error from libomrx.  Exiting.\n"); exit(1); }

int main(int argc, char *argv[]) {
    struct omrx *omrx;
    struct omrx_chunk *chunk;
    unsigned int num_points;
    float *point_data;
    unsigned int i;
    char *filename;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s filename num_points\n", argv[0]);
        return 1;
    }

    filename = argv[1];
    num_points = strtoul(argv[2], NULL, 10);

    // Generate some data to write...

    point_data = malloc(sizeof(float) * 3 * num_points);
    for (i = 0; i < num_points; i++) {
        point_data[i * 3] = (float)i;
        point_data[i * 3 + 1] = (float)(i + 1);
        point_data[i * 3 + 2] = (float)(i + 2);
    }

    // Start of libomrx-related code

    if (!omrx_init()) {
        fprintf(stderr, "omrx_init failed!\n");
        return 1;
    }

    omrx = omrx_new();
    if (!omrx) exit(1);

    chunk = omrx_get_root_chunk(omrx);
    // Add a toplevel mESH chunk with id="test"
    CHECK_OMRX_ERR(omrx_add_chunk(chunk, "mESH", &chunk));
    CHECK_OMRX_ERR(omrx_set_attr_str(chunk, OMRX_ATTR_ID, OMRX_COPY, "test"));

    // Add a VRTx chunk under mESH with some vertex data
    CHECK_OMRX_ERR(omrx_add_chunk(chunk, "VRTx", &chunk));
    CHECK_OMRX_ERR(omrx_set_attr_float32_array(chunk, OMRX_ATTR_DATA, OMRX_OWN, 3, num_points, point_data));

    // Write it out
    CHECK_OMRX_ERR(omrx_write(omrx, filename));

    return 0;
}
