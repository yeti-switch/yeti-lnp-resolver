MACRO(ADD_PKG_TARGET target)
	set(ROOT_DIR ${CMAKE_SOURCE_DIR})
	set(BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/packages/${target})

	include(${PROJECT_BINARY_DIR}/${target}/manifest.txt)

	configure_file(packages/package-${target}.txt ${BUILD_DIR}/CMakeLists.txt @ONLY)
	configure_file(packages/common.txt.in ${BUILD_DIR}/common.txt @ONLY)

	#copy all files from folders like "packages/${target} (used for extra control files)
	if(EXISTS ${CMAKE_SOURCE_DIR}/packages/${target})
		file(GLOB EXTRA_FILES packages/${target}/*)
		foreach(extra_file ${EXTRA_FILES})
			file(COPY ${extra_file} DESTINATION ${BUILD_DIR})
		endforeach(extra_file)
	endif(EXISTS ${CMAKE_SOURCE_DIR}/packages/${target})

	add_custom_target(package-${target}
		COMMAND ${CMAKE_COMMAND} . 
		COMMAND ${CMAKE_CPACK_COMMAND} -G DEB
		COMMAND cp *.deb ${PROJECT_BINARY_DIR}
		WORKING_DIRECTORY ${BUILD_DIR}
	)
ENDMACRO(ADD_PKG_TARGET target)
