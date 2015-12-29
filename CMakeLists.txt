cmake_minimum_required (VERSION 2.6)
project (libomrx)

include_directories ("${PROJECT_SOURCE_DIR}/include")

set(LIBOMRX_LIB_NAME omrx)
set(LIBOMRX_VERSION 0.1)

if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR lib)
endif(NOT DEFINED CMAKE_INSTALL_LIBDIR)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to RelWithDebInfo")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif(NOT CMAKE_BUILD_TYPE)

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
if(DEFINED INSTALL_DOCS)
    option(INSTALL_DOCS "Install API documentation" ${INSTALL_DOCS})
else()
    option(INSTALL_DOCS "Install API documentation" ON)
endif()

# TODO: make these cache/options
set(BIN_INSTALL_DIR bin)
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
set(INCLUDE_INSTALL_DIR include)
set(DOC_INSTALL_DIR doc)

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

# Targets to build/install libraries

if(LIBOMRX_SHARED)
    add_library(${LIBOMRX_LIB_NAME} SHARED ${libomrx_sources})
    if(MSVC)
        # msvc does not append 'lib' - do it here to have consistent name
        set_target_properties(${LIBOMRX_LIB_NAME} PROPERTIES PREFIX "lib")
        set_target_properties(${LIBOMRX_LIB_NAME} PROPERTIES IMPORT_PREFIX "lib")
    endif(MSVC)

    install(
        TARGETS ${LIBOMRX_LIB_NAME}
        EXPORT libomrx_targets
        RUNTIME DESTINATION ${BIN_INSTALL_DIR}
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
        INCLUDES DESTINATION ${INCLUDE_INSTALL_DIR}
    )
endif(LIBOMRX_SHARED)

if(LIBOMRX_STATIC)
    # does not work without changing name
    set(LIBOMRX_LIB_NAME_STATIC ${LIBOMRX_LIB_NAME}_static)
    add_library(${LIBOMRX_LIB_NAME_STATIC} STATIC ${libomrx_sources})
    if(MSVC)
        # msvc does not append 'lib' - do it here to have consistent name
        set_target_properties(
            ${LIBOMRX_LIB_NAME_STATIC}
            PROPERTIES
                PREFIX "lib"
        )
    else(MSVC)
        # MSVC doesn't use a different file extension for shared vs. static
        # libs.  We are able to change OUTPUT_NAME to remove the _static
        # for all other platforms.
        set_target_properties(
            ${LIBOMRX_LIB_NAME_STATIC}
            PROPERTIES
                OUTPUT_NAME ${LIBOMRX_LIB_NAME}
                CLEAN_DIRECT_OUTPUT 1
        )
    endif(MSVC)

    install(
        TARGETS ${LIBOMRX_LIB_NAME_STATIC}
        EXPORT libomrx_targets
        RUNTIME DESTINATION ${BIN_INSTALL_DIR}
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
        INCLUDES DESTINATION ${INCLUDE_INSTALL_DIR}
    )
endif(LIBOMRX_STATIC)

# Install public headers

install(
    FILES ${libomrx_public_headers}
    DESTINATION ${INCLUDE_INSTALL_DIR}
)

# Targets to generate documentation using Doxygen:
#   doc -- API docs in doc/api (included in 'make all')
#   doc-internal -- internal docs in doc/internal (not part of 'make all')

find_package(Doxygen)
if(DOXYGEN_FOUND)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc)

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.api.in
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.api
        @ONLY
    )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/api/.build.timestamp
        DEPENDS ${libomrx_public_headers} ${libomrx_sources}
        COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile.api > /dev/null
        COMMAND touch doc/api/.build.timestamp
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
    add_custom_target(
        doc ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/api/.build.timestamp
    )

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.internal.in
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.internal
        @ONLY
    )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/internal/.build.timestamp
        DEPENDS ${libomrx_public_headers} ${libomrx_sources}
        COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile.internal > /dev/null
        COMMAND touch doc/internal/.build.timestamp
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating internal docs with Doxygen" VERBATIM
    )
    add_custom_target(
        doc-internal
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/internal/.build.timestamp
    )

    set_directory_properties(
        PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
            "${CMAKE_CURRENT_BINARY_DIR}/doc/api;${CMAKE_CURRENT_BINARY_DIR}/doc/internal"
    )

    if(INSTALL_DOCS)
        install(
            DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/api/html/
            DESTINATION ${DOC_INSTALL_DIR}/${CMAKE_PROJECT_NAME}
        )
    endif(INSTALL_DOCS)

endif(DOXYGEN_FOUND)

# Create/install cmake config/version files for find_package()

include(CMakePackageConfigHelpers)

configure_package_config_file(
    libomrx-config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/libomrx-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/libomrx/cmake
    PATH_VARS INCLUDE_INSTALL_DIR LIB_INSTALL_DIR
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/libomrx-config-version.cmake
    VERSION ${LIBOMRX_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libomrx-config.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/libomrx-config-version.cmake
    DESTINATION ${LIB_INSTALL_DIR}/cmake/libomrx
)

# Create/install libomrx.pc file for pkg-config

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/libomrx.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/libomrx.pc
    @ONLY
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libomrx.pc
    DESTINATION ${LIB_INSTALL_DIR}/pkgconfig
)

# Include CMakeList.txt from 'apps' subdirectory to build apps/programs

add_subdirectory(apps)
