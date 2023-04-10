set(BIN_NAME yeti_lnp_resolver)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(PQXX REQUIRED)

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
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/third/prometheus-cpp
    CMAKE_ARGS -DBUILD_SHARED_LIBS=ON -DENABLE_PUSH=OFF -DENABLE_COMPRESSION=OFF -DGENERATE_PKG_CONFIG=OFF)

    FetchContent_GetProperties(prometheus-cpp)
    if(NOT prometheus-cpp_POPULATED)
        FetchContent_Populate(prometheus-cpp)
        add_subdirectory(${prometheus-cpp_SOURCE_DIR} ${prometheus-cpp_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-write-strings -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=")

include_directories(
    ${PQXX_INCLUDE_DIRECTORIES}
    ${LIBRE_BUNDLED_INCLUDE_DIRS}
    ${CONFUSE_BUNDLED_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

file(GLOB_RECURSE SRCS *.cpp *.c)
add_executable (${BIN_NAME} ${SRCS})
target_link_libraries(${BIN_NAME}
    ${CMAKE_THREAD_LIBS_INIT}
    ${PQXX_LIBRARIES}
    ${LIBRE_BUNDLED_LIBRARIES}
    ${CONFUSE_BUNDLED_LIBS}
    ${CURL_LIBRARIES}
    ${PROMETHEUS_LIBRARIES})

add_dependencies(${BIN_NAME} libre)
add_dependencies(${BIN_NAME} libconfuse)

install(TARGETS ${BIN_NAME} DESTINATION /usr/bin)
