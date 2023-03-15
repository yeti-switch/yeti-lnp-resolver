cmake_minimum_required (VERSION 2.8)

project(yeti-lnp-resolver)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS} -g")

set(RUNTIME_DIR /usr/bin)
set(LIB_DIR /usr/lib)
set(HDRS_DIR /usr/include/yeti)
set(CFG_DIR /etc/yeti)
set(DEF_CFG_DIR /etc/default)
set(SYSLOG_CFG_DIR /etc/rsyslog.d)
set(INIT_DIR /etc/init.d)

option(VERBOSE_LOGGING "Compile with verbose logging (file,lineno,func)" ON)

#get version
if(EXISTS ${CMAKE_SOURCE_DIR}/.git)
        EXECUTE_PROCESS(
                COMMAND "git" "describe" "--tags" "--abbrev=0"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE TAG_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if("${TAG_VERSION}" STREQUAL "")
            set(TAG_VERSION "0.0.0")
        endif("${TAG_VERSION}" STREQUAL "")
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
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVERBOSE_LOGGING -DCFG_DIR='\"${CFG_DIR}\"'")
endif(VERBOSE_LOGGING)

add_subdirectory(server)

#packaging
set(DEF_FILES ./debian/yeti_lnp_resolver)
set(SYSLOG_FILES ./debian/yeti-lnp-resolver.conf)
set(SERVICE_FILES ./debian/yeti-lnp-resolver.service)

#process init.d default files
FOREACH(def_path ${DEF_FILES})
	get_filename_component(def_name ${def_path} NAME)
	install(FILES ${def_path} DESTINATION "@DEF_CFG_DIR@")
	file(APPEND conffiles "@DEF_CFG_DIR@/${def_name}\n")
ENDFOREACH(def_path)

#process syslog files
FOREACH(cfg_path ${SYSLOG_FILES})
	get_filename_component(cfg_name ${cfg_path} NAME)
	install(FILES ${cfg_path} DESTINATION "@SYSLOG_CFG_DIR@")
	file(APPEND conffiles "@SYSLOG_CFG_DIR@/${cfg_name}\n")
ENDFOREACH(cfg_path)

#process systemd unit files
FOREACH(service_path ${SERVICE_FILES})
    install(PROGRAMS ${service_path} DESTINATION /lib/systemd/system)
ENDFOREACH(service_path)
