#.rst:
# FindNanoMsg
# --------
#
# Find NanoMsg
#
# Find NanoMsg headers and libraries.
#
# ::
#
#   NanoMsg_LIBRARIES      - List of libraries when using NanoMsg.
#   NanoMsg_FOUND          - True if libnanomsg found.
#   NanoMsg_VERSION        - Version of found libnanomsg.

find_package(PkgConfig REQUIRED)
# Supported for debian Stretch/9
pkg_check_modules(NanoMsg libnanomsg)
if(NOT NanoMsg_FOUND)
    # Supported for debian Buster/10
    pkg_check_modules(NanoMsg nanomsg)
endif(NOT NanoMsg_FOUND)

# handle the QUIETLY and REQUIRED arguments and set NanoMsg_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
    NanoMsg
    REQUIRED_VARS NanoMsg_LIBRARIES
    VERSION_VAR NanoMsg_VERSION)

