cmake_minimum_required (VERSION 2.8)

cmake_policy(SET CMP0026 OLD)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(RUNTIME_DIR /usr/bin)
set(LIB_DIR /usr/lib)
set(HDRS_DIR /usr/include/yeti)
set(CFG_DIR /etc/yeti)

option(VERBOSE_LOGGING "Compile with verbose logging (file,lineno,func)" ON)

#get version
if(EXISTS ${CMAKE_SOURCE_DIR}/.git)
        EXECUTE_PROCESS(
                COMMAND "git" "describe" "--tags" "--abbrev=0"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE TAG_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        MESSAGE(STATUS "got version from git tags: " ${TAG_VERSION})
        STRING(REGEX REPLACE "[-\\.]" ";" TAG_VERSION_LIST ${TAG_VERSION})
else(EXISTS ${CMAKE_SOURCE_DIR}/.git)
        MESSAGE(FATAL_ERROR "git repository required")
endif(EXISTS ${CMAKE_SOURCE_DIR}/.git)

list(GET TAG_VERSION_LIST 0 MAJOR_VERSION)
list(GET TAG_VERSION_LIST 1 MINOR_VERSION)
list(GET TAG_VERSION_LIST 2 PATCH_VERSION)
#LIST(GET TAG_VERSION_LIST 3 BUILD_VERSION)

if(VERBOSE_LOGGING)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVERBOSE_LOGGING -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"' -DCFG_DIR='\"${CFG_DIR}\"'")
endif(VERBOSE_LOGGING)

add_subdirectory(server)

#packaging
include(${CMAKE_SOURCE_DIR}/cmake/pkg_target.cmake)
add_pkg_target(server)
add_custom_target(deb DEPENDS package-server)
