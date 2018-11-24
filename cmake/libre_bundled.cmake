MESSAGE(STATUS "Enable building of the bundled Curl")

set (LIBRE_DIR third/re)
set (LIBRE_SRC_DIR ${PROJECT_SOURCE_DIR}/${LIBRE_DIR})
set (LIBRE_PATCH_FILE ${PROJECT_SOURCE_DIR}/third/libre_disable_redirect.patch)
set (LIBRE_BIN_DIR ${PROJECT_BINARY_DIR}/${LIBRE_DIR})
set (LIBRE_BUNDLED_LIB ${LIBRE_BIN_DIR}/libre.a)

add_custom_target(libre ALL DEPENDS ${LIBRE_BUNDLED_LIB})

file(MAKE_DIRECTORY ${LIBRE_BIN_DIR})

add_custom_command(OUTPUT ${LIBRE_BUNDLED_LIB}
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBRE_SRC_DIR} ${LIBRE_BIN_DIR}
    COMMAND git apply ${LIBRE_PATCH_FILE}
    COMMAND $(MAKE)
    WORKING_DIRECTORY ${LIBRE_BIN_DIR})

set(LIBRE_BUNDLED_INCLUDE_DIRS ${LIBRE_BIN_DIR}/include)

add_library(libre_bundled STATIC IMPORTED)
set_property(TARGET libre_bundled PROPERTY IMPORTED_LOCATION ${LIBRE_BUNDLED_LIB})
set(LIBRE_BUNDLED_LIBRARIES -lssl -lcrypto -lz ${LIBRE_BUNDLED_LIB})
