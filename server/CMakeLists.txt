cmake_minimum_required (VERSION 2.8)
cmake_policy(SET CMP0026 OLD)
set(BIN_NAME yeti_lnp_resolver)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(PQXX REQUIRED)
find_package(NanoMsg REQUIRED)
find_package(Confuse REQUIRED)
find_package(YetiCC REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-write-strings")

include_directories(
	${PQXX_INCLUDE_DIRECTORIES}
	${LIBRE_BUNDLED_INCLUDE_DIRS}
	${CURL_INCLUDE_DIRS}
	${YETICC_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

file(GLOB_RECURSE SRCS src/*.cpp src/*.c)
add_executable (${BIN_NAME} ${SRCS})
target_link_libraries(${BIN_NAME}
	${CMAKE_THREAD_LIBS_INIT}
	${PQXX_LIBRARIES}
	${LIBRE_BUNDLED_LIBRARIES}
	${NanoMsg_LIBRARIES}
	${Confuse_LIBRARIES}
	${CURL_LIBRARIES}
	${YETICC_LIBRARIES})

install(TARGETS ${BIN_NAME} DESTINATION ${RUNTIME_DIR})

get_property(TARGET_BINS TARGET ${BIN_NAME} PROPERTY LOCATION)

file(GLOB_RECURSE TARGET_CFGS ${CMAKE_CURRENT_SOURCE_DIR}/etc/*)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/manifest.txt.in ${CMAKE_CURRENT_BINARY_DIR}/manifest.txt)
