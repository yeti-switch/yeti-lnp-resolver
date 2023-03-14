set(BIN_NAME yeti_lnp_resolver)

include(../../cmake/libre_bundled.cmake)
include(../../cmake/confuse_bundled.cmake)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(PQXX REQUIRED)
find_package(NanoMsg REQUIRED)

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
	${YETICC_LIBRARIES})

add_dependencies(${BIN_NAME} libre)
add_dependencies(${BIN_NAME} libconfuse)

install(TARGETS ${BIN_NAME} DESTINATION ${RUNTIME_DIR})

get_property(TARGET_BINS TARGET ${BIN_NAME} PROPERTY LOCATION)

file(GLOB_RECURSE TARGET_CFGS ${CMAKE_CURRENT_SOURCE_DIR}/etc/*)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../manifest.txt.in ${CMAKE_CURRENT_BINARY_DIR}/../manifest.txt)
