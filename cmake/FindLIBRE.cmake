# Find LIBRE library and header file
# Sets
#   LIBRE_FOUND               to 0 or 1 depending on the result
#   LIBRE_INCLUDE_DIRECTORIES to directories required for using libre
#   LIBRE_LIBRARIES           to libre and any dependent libraries
# If LIBRE_REQUIRED is defined, then a fatal error message will be generated if libre is not found

if ( NOT LIBRE_INCLUDE_DIRECTORIES OR NOT LIBRE_LIBRARIES OR NOT LIBRE_FOUND )

  if ( $ENV{LIBRE_DIR} )
    file( TO_CMAKE_PATH "$ENV{LIBRE_DIR}" _LIBRE_DIR )
  endif ( $ENV{LIBRE_DIR} )

  find_library( LIBRE_LIBRARIES
    NAMES re
    PATHS
      ${_LIBRE_DIR}/lib64
      ${CMAKE_INSTALL_PREFIX}/lib64
      /usr/local/lib64
      /usr/lib/x86_64-linux-gnu
      /usr/lib64
      ${_LIBRE_DIR}
      ${_LIBRE_DIR}/lib
      ${CMAKE_INSTALL_PREFIX}/bin
      ${CMAKE_INSTALL_PREFIX}/lib
      /usr/local/lib
      /usr/lib
    NO_DEFAULT_PATH
  )

  find_path( LIBRE_INCLUDE_DIRECTORIES
    NAMES re.h
    PATHS
      /usr/include/re
      ${_LIBRE_DIR}
      ${_LIBRE_DIR}/include
      ${CMAKE_INSTALL_PREFIX}/include
      /usr/local/include
      /usr/include
      /usr/include/postgresql
    NO_DEFAULT_PATH
  )

  if ( NOT LIBRE_INCLUDE_DIRECTORIES OR NOT LIBRE_LIBRARIES ) 
    if ( LIBRE_REQUIRED )
      message( FATAL_ERROR "LIBRE is required. Set LIBRE_DIR" )
    endif ( LIBRE_REQUIRED )
  else ( NOT LIBRE_INCLUDE_DIRECTORIES OR NOT LIBRE_LIBRARIES ) 
    set( LIBRE_FOUND 1 )
    mark_as_advanced( LIBRE_FOUND )
  endif ( NOT LIBRE_INCLUDE_DIRECTORIES OR NOT LIBRE_LIBRARIES )

endif ( NOT LIBRE_INCLUDE_DIRECTORIES OR NOT LIBRE_LIBRARIES OR NOT LIBRE_FOUND )

mark_as_advanced( FORCE LIBRE_INCLUDE_DIRECTORIES )
mark_as_advanced( FORCE LIBRE_LIBRARIES )
