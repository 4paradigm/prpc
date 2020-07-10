include(common)

find_lib(PicoCoreDep_STATIC_LIBRARIES STATIC LIBS prometheus-cpp-pull prometheus-cpp-core glog gflags boost_regex yaml-cpp boost_iostreams boost_system boost_filesystem boost_thread zookeeper_mt z snappy lz4)
find_lib(PicoCoreDep_LIBRARIES        SHARED LIBS prometheus-cpp-pull prometheus-cpp-core glog gflags boost_regex yaml-cpp boost_iostreams boost_system boost_filesystem boost_thread zookeeper_mt z snappy lz4)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PicoCoreDep DEFAULT_MSG PicoCoreDep_LIBRARIES)
find_package_handle_standard_args(PicoCoreDep_STATIC DEFAULT_MSG PicoCoreDep_STATIC_LIBRARIES)
mark_as_advanced(PicoCoreDep_LIBRARIES)
mark_as_advanced(PicoCoreDep_STATIC_LIBRARIES)
