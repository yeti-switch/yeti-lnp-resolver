cmake_minimum_required (VERSION 3.14)

project(yeti-lnp-resolver)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_STANDARD 14)

include(cmake/libre_bundled.cmake)
include(cmake/confuse_bundled.cmake)

set(CFG_DIR /etc/yeti)

option(VERBOSE_LOGGING "Compile with verbose logging (file,lineno,func)" ON)

#get version


IF(EXISTS ${CMAKE_SOURCE_DIR}/debian/changelog)
    file(STRINGS ${CMAKE_SOURCE_DIR}/debian/changelog CHANGELOG_LINE LIMIT_COUNT 1)
    string(REGEX REPLACE "^[^(]*\\(([0-9]+)\\.([0-9]+)\\.([0-9a-z]+)\\).*" "\\1.\\2.\\3" BUILD_VERSION ${CHANGELOG_LINE})
    set(MAJOR_VERSION ${CMAKE_MATCH_1})
    set(MINOR_VERSION ${CMAKE_MATCH_2})
    set(PATCH_VERSION ${CMAKE_MATCH_3})

    message(STATUS "Version from changelog: ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")
ELSE(EXISTS ${CMAKE_SOURCE_DIR}/debian/changelog)
    MESSAGE(FATAL_ERROR "debian/changelog not found")
ENDIF(EXISTS ${CMAKE_SOURCE_DIR}/debian/changelog)

if(VERBOSE_LOGGING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVERBOSE_LOGGING -DCFG_DIR='\"${CFG_DIR}\"'")
endif(VERBOSE_LOGGING)

add_subdirectory(src)
add_subdirectory(debian)

install(FILES etc/lnp_resolver.cfg.dist DESTINATION /etc/yeti)
