set(BIN_NAME yeti_lnp_resolver)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(PQXX REQUIRED)
find_package(NanoMsg REQUIRED)

#prometheus
set(PROMETHEUS_LIBRARIES prometheus-cpp::pull)
find_package(prometheus-cpp CONFIG QUIET)
if (prometheus-cpp_FOUND)
    message("-- prometheus-cpp package was found")
else()
    message("-- prometheus-cpp not found, fetch library from github")
    include(FetchContent)
    FetchContent_Declare(
	prometheus-cpp
	GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
	GIT_TAG v1.1.0
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/third/prometheus-cpp/
	CMAKE_ARGS -DBUILD_SHARED_LIBS=ON -DENABLE_PUSH=OFF -DENABLE_COMPRESSION=OFF)

    FetchContent_MakeAvailable(prometheus-cpp)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-write-strings -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=")

include_directories(
    ${PQXX_INCLUDE_DIRECTORIES}
    ${LIBRE_BUNDLED_INCLUDE_DIRS}
    ${CONFUSE_BUNDLED_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${YETICC_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

file(GLOB_RECURSE SRCS *.cpp *.c)
add_executable (${BIN_NAME} ${SRCS})
target_link_libraries(${BIN_NAME}
    ${CMAKE_THREAD_LIBS_INIT}
    ${PQXX_LIBRARIES}
    ${LIBRE_BUNDLED_LIBRARIES}
    ${CONFUSE_BUNDLED_LIBS}
    ${NanoMsg_LIBRARIES}
    ${Confuse_LIBRARIES}
    ${CURL_LIBRARIES}
    ${YETICC_LIBRARIES}
    ${PROMETHEUS_LIBRARIES})

install(TARGETS ${BIN_NAME} DESTINATION ${RUNTIME_DIR})

# workaround for support newer CMake 3.14 version
# push the current (NEW) CMake policy onto the stack, and apply the OLD policy.
cmake_policy(PUSH)
cmake_policy(SET CMP0026 OLD)

# obsolete method: "the Location property may not be read from target"
get_property(TARGET_BINS TARGET ${BIN_NAME} PROPERTY LOCATION)

# pop the previous policy from the stack to re-apply the NEW behavior.
cmake_policy(POP)

file(GLOB_RECURSE TARGET_CFGS ${CMAKE_CURRENT_SOURCE_DIR}/etc/*)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../manifest.txt.in ${CMAKE_CURRENT_BINARY_DIR}/../manifest.txt)
