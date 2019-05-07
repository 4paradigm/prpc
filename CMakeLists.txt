cmake_minimum_required(VERSION 3.10)
file(GLOB SRC src/*.cpp)
find_package(Jemalloc REQUIRED)
find_package(GLOG REQUIRED)
find_package(GFLAGS REQUIRED)
find_package(PicoCommonDep REQUIRED)

set(COMMON_LIB ${PicoCommonDep_LIBRARIES} )

set(MALLOC_DEFINITIONS -DUSE_JEMALLOC -DOVERRIDE_SYSTEM_MALLOC=0 -DOVERRIDE_SYSTEM_NEW=0)
add_library(pico_common ${SRC})
target_compile_definitions(pico_common PRIVATE ${MALLOC_DEFINITIONS})
target_compile_options(pico_common PUBLIC)
target_link_libraries(pico_common PUBLIC ${Jemalloc_pic_LIBRARIES} ${RDMA_LIBRARIES} ${COMMON_LIB} dl)

add_executable(masterd src/masterd.cc)
target_link_libraries(masterd pico_common pico_profiler)

add_subdirectory(test)
