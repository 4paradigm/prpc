cmake_minimum_required(VERSION 3.10)
file(GLOB SRC src/*.cpp)
find_package(Jemalloc REQUIRED)
find_package(GLOG REQUIRED)
find_package(GFLAGS REQUIRED)
find_package(PicoCommonDep REQUIRED)

add_definitions(--std=c++14 -Wall -Wextra -Wno-deprecated-declarations -frecord-gcc-switches -fPIC)

option(USE_RDMA "whether build with rdma support" OFF)
if (USE_RDMA)
    add_definitions(-DUSE_RDMA)
    set(RDMA_LIBRARIES rdmacm ibverbs)
    message(STATUS "RDMA enabled")
else()
    message(STATUS "RDMA disabled")
    set(RDMA_LIBRARIES )
endif()

if (DEBUG)
    add_definitions(-O0 -g)
else()
    add_definitions(-O3 -DNDEBUG)
endif()

set(CMAKE_SHARED_LINKER_FLAGS "-pthread -Wl,--whole-archive -lrt -Wl,--no-whole-archive")
set(CMAKE_EXE_LINKER_FLAGS "-pthread -Wl,--whole-archive -lrt -Wl,--no-whole-archive")

include_directories(SYSTEM src third-party/include)
link_directories(third-party/lib third-party/lib64)
set(COMMON_LIB ${PicoCommonDep_LIBRARIES} )

set(MALLOC_DEFINITIONS -DUSE_JEMALLOC -DOVERRIDE_SYSTEM_MALLOC=0 -DOVERRIDE_SYSTEM_NEW=0)

add_library(pico_common_obj OBJECT ${SRC})

add_library(pico_common SHARED $<TARGET_OBJECTS:pico_common_obj>)
target_compile_definitions(pico_common PRIVATE ${MALLOC_DEFINITIONS})
target_link_libraries(pico_common PUBLIC ${Jemalloc_pic_LIBRARIES} ${RDMA_LIBRARIES} ${COMMON_LIB} dl)
add_dependencies(pico_common pico_common_obj)


add_library(pico_common_static STATIC  $<TARGET_OBJECTS:pico_common_obj>)
target_compile_definitions(pico_common_static PRIVATE ${MALLOC_DEFINITIONS})
target_link_libraries(pico_common_static PUBLIC ${Jemalloc_pic_LIBRARIES} ${RDMA_LIBRARIES} ${COMMON_LIB} dl)
add_dependencies(pico_common_static pico_common_obj)


add_executable(masterd src/masterd.cc)
target_link_libraries(masterd pico_common_static)

add_subdirectory(test)
