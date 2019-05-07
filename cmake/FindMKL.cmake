# This module defines the following variables:
#
#   MKL_FOUND            : True mkl is found
#   MKL_INCLUDE_DIR      : unclude directory
#   MKL_LIBRARIES        : the libraries to link against.
#
#if (DEFINED ENV{MKLROOT})
#    set(MKLROOT $ENV{MKLROOT})
#endif ()
include(common)

find_include(MKL_INCLUDE HEADERS mkl_cblas.h PATHS ${MKLROOT}/include /opt/intel/mkl/include)
if (APPLE)
    find_lib(MKL_LIB STATIC
            LIBS mkl_intel_lp64 mkl_sequential mkl_core
            PATHS ${MKLROOT}/lib ${MKLROOT}/lib/intel64 $ENV{MKLROOT}/lib)
else ()
    find_lib(MKL_LIB STATIC
            LIBS mkl_intel_lp64 mkl_gnu_thread mkl_core mkl_gnu_thread
            PATHS ${MKLROOT}/lib ${MKLROOT}/lib/intel64 $ENV{MKLROOT}/lib)
endif ()
#find_lib(MKL_LIB STATIC
#        LIBS mkl_intel_lp64 mkl_intel_thread mkl_core mkl_intel_thread iomp5
#        PATHS ${MKLROOT}/lib ${MKLROOT}/lib/intel64 $ENV{MKLROOT}/lib)
if (MKL_LIB AND MKL_INCLUDE)
    set(HAVE_MKL TRUE)
endif ()

if (HAVE_MKL)
    #    message(STATUS "Intel(R) MKL found: include ${MKL_INCLUDE}, lib ${MKL_LIB} ")
    set(MKL_LIBRARIES ${MKL_LIB})
    set(MKL_INCLUDE_DIR ${MKL_INCLUDE})

else ()
    message(WARNING "Intel(R) MKL not found. Some performance features may not be "
            "available. Please run scripts/prepare_mkl.sh to download a minimal "
            "set of libraries or get a full version from "
            "https://software.intel.com/en-us/intel-mkl")
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MKL DEFAULT_MSG MKL_LIBRARIES MKL_INCLUDE_DIR)


#set(MKL_LINK_FLAGS " -L${MKL_LIB} -Wl,-rpath,${MKL_LIB} -lmkl_rt -lpthread -lm -ldl -fopenmp")
#set(MKL_COMPILE_FLAGS "  -I${MKL_INCLUDES} ")

mark_as_advanced(MKL_LIBRARIES MKL_INCLUDE_DIR)

