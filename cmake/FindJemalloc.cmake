include(common)

find_lib(Jemalloc_LIBRARIES STATIC LIBS jemalloc)
find_lib(Jemalloc_pic_LIBRARIES STATIC LIBS jemalloc_pic)
find_include(Jemalloc_INCLUDE_DIR HEADERS jemalloc/jemalloc.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jemalloc DEFAULT_MSG
        Jemalloc_LIBRARIES Jemalloc_pic_LIBRARIES Jemalloc_INCLUDE_DIR)
mark_as_advanced(Jemalloc_LIBRARIES Jemalloc_pic_LIBRARIES Jemalloc_INCLUDE_DIR)
#mark_as_advanced(Jemalloc_INCLUDES)
