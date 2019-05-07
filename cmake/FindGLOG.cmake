include(common)

find_lib(GLOG_LIBRARIES STATIC LIBS glog)
find_include(GLOG_INCLUDE_DIR HEADERS glog/logging.h)
include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(GLOG DEFAULT_MSG GLOG_LIBRARIES GLOG_INCLUDE_DIR)
mark_as_advanced(GLOG_LIBRARIES GLOG_INCLUDE_DIR)
