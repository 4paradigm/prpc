include(common)

find_lib(PicoCoreDep_LIBRARIES STATIC LIBS glog gflags boost_regex yaml-cpp boost_iostreams boost_system boost_thread zookeeper_mt z snappy lz4)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PicoCoreDep DEFAULT_MSG PicoCoreDep_LIBRARIES)
mark_as_advanced(PicoCoreDep_LIBRARIES)
