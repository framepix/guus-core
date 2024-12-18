# FindIntX.cmake
# Locate IntX library for use within a Monero fork with EVMone integration
# This module defines:
#  INTX_FOUND, if false, do not try to link to IntX
#  INTX_INCLUDE_DIR, where to find the headers
#  INTX_LIBRARY, the library to link against

# Search for the IntX header file
find_path(INTX_INCLUDE_DIR
  NAMES intx/intx.hpp
  HINTS
    ${PROJECT_SOURCE_DIR}/external/intx/include  # Check if IntX is built here
    $ENV{INTX_ROOT}/include  # If INTX_ROOT env variable is set
  PATHS
    /usr/local/include
    /usr/include
  DOC "The directory where IntX headers reside"
)

# Look for the IntX static or shared library
find_library(INTX_LIBRARY
  NAMES intx
  HINTS
    ${PROJECT_SOURCE_DIR}/external/intx/lib  # Check if IntX library is here
    $ENV{INTX_ROOT}/lib  # If INTX_ROOT env variable is set
  PATHS
    /usr/local/lib
    /usr/lib
  DOC "The IntX library"
)

#
