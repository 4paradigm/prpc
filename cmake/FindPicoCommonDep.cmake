include(common)

find_lib(PicoCommonDep_LIBRARIES STATIC LIBS glog gflags boost_regex yaml-cpp boost_iostreams boost_system boost_thread zookeeper_mt z snappy lz4)
if(NOT APPLE)
    find_lib(PicoCommonDep_LIBRARIES STATIC LIBS  unwind)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PicoCommonDep DEFAULT_MSG PicoCommonDep_LIBRARIES)
mark_as_advanced(PicoCommonDep_LIBRARIES)
