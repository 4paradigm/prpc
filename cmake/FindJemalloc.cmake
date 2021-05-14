include(common)

find_lib(Jemalloc_STATIC_LIBRARIES STATIC LIBS jemalloc)
set(Jemalloc_STATIC_LIBRARIES ${Jemalloc_STATIC_LIBRARIES} rt)
find_lib(Jemalloc_pic_STATIC_LIBRARIES STATIC LIBS jemalloc_pic)
set(Jemalloc_pic_STATIC_LIBRARIES ${Jemalloc_pic_STATIC_LIBRARIES} rt)
find_lib(Jemalloc_LIBRARIES SHARED LIBS jemalloc)
set(Jemalloc_LIBRARIES ${Jemalloc_LIBRARIES} rt)
find_lib(Jemalloc_pic_LIBRARIES SHARED LIBS jemalloc)
set(Jemalloc_pic_LIBRARIES ${Jemalloc_pic_LIBRARIES} rt)

find_include(Jemalloc_INCLUDE_DIR HEADERS jemalloc/jemalloc.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jemalloc DEFAULT_MSG
        Jemalloc_LIBRARIES Jemalloc_pic_LIBRARIES Jemalloc_INCLUDE_DIR)
find_package_handle_standard_args(Jemalloc_STATIC DEFAULT_MSG
        Jemalloc_STATIC_LIBRARIES Jemalloc_pic_STATIC_LIBRARIES Jemalloc_INCLUDE_DIR)

mark_as_advanced(Jemalloc_LIBRARIES Jemalloc_pic_LIBRARIES Jemalloc_INCLUDE_DIR)
mark_as_advanced(Jemalloc_STATIC_LIBRARIES Jemalloc_pic_STATIC_LIBRARIES Jemalloc_INCLUDE_DIR)
#mark_as_advanced(Jemalloc_INCLUDES)
