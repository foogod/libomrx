#include <stdio.h>
#include <stdlib.h>

#include "omrx.h"

#define CHECK_OMRX_ERR(x) if ((x) < 0) { exit(1); }

int main(int argc, char *argv[]) {
    struct omrx *omrx;
    struct omrx_chunk *chunk;
    unsigned int num_points;
    float *point_data;

    if (!omrx_init()) {
        fprintf(stderr, "omrx_init failed!\n");
        exit(1);
    }

    num_points = strtoul(argv[2], NULL, 10);
    point_data = malloc(sizeof(float) * 3 * num_points);
    // ... fill in data ...

    omrx = omrx_new();
    if (!omrx) exit(1);

    chunk = omrx_get_root_chunk(omrx);
    CHECK_OMRX_ERR(omrx_add_chunk(chunk, "MESH", &chunk));
    CHECK_OMRX_ERR(omrx_set_attr_str(chunk, OMRX_ATTR_ID, OMRX_COPY, "test"));
    CHECK_OMRX_ERR(omrx_add_chunk(chunk, "VRTx", &chunk));
    CHECK_OMRX_ERR(omrx_set_attr_float_array(chunk, OMRX_ATTR_DATA, OMRX_OWN, 3, num_points, point_data));

    CHECK_OMRX_ERR(omrx_write(omrx, argv[1]));

    return 0;
}
