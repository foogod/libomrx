cmake_minimum_required (VERSION 2.6)
project (libomrx)
include_directories ("${PROJECT_SOURCE_DIR}/include")

set(LIBOMRX_LIB_NAME omrx)

# Options / Flags

if(DEFINED LIBOMRX_SHARED)
    option(LIBOMRX_SHARED "Build shared lib" ${LIBOMRX_SHARED})
else()
    option(LIBOMRX_SHARED "Build shared lib" ON)
endif()
if(DEFINED LIBOMRX_STATIC)
    option(LIBOMRX_STATIC "Build static lib" ${LIBOMRX_STATIC})
else()
    option(LIBOMRX_STATIC "Build static lib" ON)
endif()

# Sources

set(libomrx_public_headers
    include/omrx.h
)
set(libomrx_sources
    src/libomrx.c
)

# Output dirs

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Targets to build libraries

if(LIBOMRX_SHARED)
    add_library(${LIBOMRX_LIB_NAME} SHARED ${libomrx_sources})
    if(MSVC)
        # msvc does not append 'lib' - do it here to have consistent name
        set_target_properties(${LIBOMRX_LIB_NAME} PROPERTIES PREFIX "lib")
        set_target_properties(${LIBOMRX_LIB_NAME} PROPERTIES IMPORT_PREFIX "lib")
    endif()
    target_link_libraries(${LIBOMRX_LIB_NAME} ${ZLIB_LIBRARY} ${M_LIBRARY})
endif()

if(LIBOMRX_STATIC)
    # does not work without changing name
    set(LIBOMRX_LIB_NAME_STATIC ${LIBOMRX_LIB_NAME}_static)
    add_library(${LIBOMRX_LIB_NAME_STATIC} STATIC ${libomrx_sources})
    if(MSVC)
        # msvc does not append 'lib' - do it here to have consistent name
        set_target_properties(${LIBOMRX_LIB_NAME_STATIC} PROPERTIES PREFIX "lib")
    else()
        # MSVC doesn't use a different file extension for shared vs. static
        # libs.  We are able to change OUTPUT_NAME to remove the _static
        # for all other platforms.
        set_target_properties(${LIBOMRX_LIB_NAME_STATIC} PROPERTIES
            OUTPUT_NAME ${LIBOMRX_LIB_NAME}
            CLEAN_DIRECT_OUTPUT 1)
    endif()
    target_link_libraries(${LIBOMRX_LIB_NAME_STATIC} ${ZLIB_LIBRARY} ${M_LIBRARY})
endif()

# Targets to generate documentation using Doxygen:
#   doc -- API docs in doc/api (included in 'make all')
#   doc-internal -- internal docs in doc/internal (manual)

find_package(Doxygen)
if(DOXYGEN_FOUND)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc)

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.api.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.api @ONLY)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/api/.build.timestamp
        DEPENDS ${libomrx_public_headers} ${libomrx_sources}
        COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile.api > /dev/null
        COMMAND touch doc/api/.build.timestamp
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
    add_custom_target(doc ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/api/.build.timestamp
    )

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.internal.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.internal @ONLY)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/internal/.build.timestamp
        DEPENDS ${libomrx_public_headers} ${libomrx_sources}
        COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile.internal > /dev/null
        COMMAND touch doc/internal/.build.timestamp
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating internal docs with Doxygen" VERBATIM
    )
    add_custom_target(doc-internal
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/internal/.build.timestamp
    )

    set_directory_properties(PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/doc/api;${CMAKE_CURRENT_BINARY_DIR}/doc/internal")
endif(DOXYGEN_FOUND)

# Targets to build executables

add_executable (test_write apps/test_write.c)
target_link_libraries (test_write ${LIBOMRX_LIB_NAME})

add_executable (test_read apps/test_read.c)
target_link_libraries (test_read ${LIBOMRX_LIB_NAME})
