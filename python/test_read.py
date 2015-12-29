#!/usr/bin/python

import sys
import omrx

if len(sys.argv) != 2:
    sys.stderr.write("Usage: %s filename\n" % (sys.argv[0],))
    sys.exit(1)

filename = sys.argv[1]

# Start of libomrx-related code

o = omrx.open(filename)

# Find the mESH chunk with id="test" and get its first VRTx child
chunk = o.get_chunk_by_id("test", "mESH")
chunk = chunk.get_child("VRTx")

# Get the data from the 'data' attribute
data = chunk[omrx.OMRX_ATTR_DATA]

# Print it out
for i in xrange(len(data)):
    for j in xrange(len(data[i])):
        sys.stdout.write("%f " % (data[i][j],))
    sys.stdout.write("\n")

