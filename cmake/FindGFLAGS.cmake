include(common)

find_lib(GFLAGS_LIBRARIES STATIC LIBS gflags)
find_include(GFLAGS_INCLUDE_DIR HEADERS gflags/gflags.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GFLAGS DEFAULT_MSG GFLAGS_LIBRARIES GFLAGS_INCLUDE_DIR)
mark_as_advanced(GFLAGS_LIBRARIES GFLAGS_INCLUDE_DIR)
