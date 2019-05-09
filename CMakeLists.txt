cmake_minimum_required(VERSION 3.10)
file(GLOB SRC src/*.cpp)
find_package(Jemalloc REQUIRED)
find_package(GLOG REQUIRED)
find_package(GFLAGS REQUIRED)
find_package(PicoCommonDep REQUIRED)

add_definitions(--std=c++14 -g -Wall -Wextra -Wno-deprecated-declarations -frecord-gcc-switches -fPIC)

set(CMAKE_SHARED_LINKER_FLAGS "-pthread -Wl,--whole-archive -lrt -Wl,--no-whole-archive")
set(CMAKE_EXE_LINKER_FLAGS "-pthread -Wl,--whole-archive -lrt -Wl,--no-whole-archive")

#-static-libgcc -static-libstdc++ -pthread -Wl,--whole-archive -lrt -Wl,--no-whole-archive")

include_directories(SYSTEM src tools/include)
link_directories(tools/lib tools/lib64)
set(COMMON_LIB ${PicoCommonDep_LIBRARIES} )

set(MALLOC_DEFINITIONS -DUSE_JEMALLOC -DOVERRIDE_SYSTEM_MALLOC=0 -DOVERRIDE_SYSTEM_NEW=0)
add_library(pico_common ${SRC})
target_compile_definitions(pico_common PRIVATE ${MALLOC_DEFINITIONS})
target_compile_options(pico_common PUBLIC)
target_link_libraries(pico_common PUBLIC ${Jemalloc_pic_LIBRARIES} ${RDMA_LIBRARIES} ${COMMON_LIB} dl)

add_executable(masterd src/masterd.cc)
target_link_libraries(masterd pico_common)

add_subdirectory(test)
