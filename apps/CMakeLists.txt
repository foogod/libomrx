add_executable (test_write test_write.c)
target_link_libraries (test_write ${LIBOMRX_LIB_NAME})

add_executable (test_read test_read.c)
target_link_libraries (test_read ${LIBOMRX_LIB_NAME})

