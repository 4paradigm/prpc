cmake_minimum_required(VERSION 3.10)

if(NOT THIRD_PARTY)
    set(THIRD_PARTY "third-party")
endif()
message(STATUS "third party: ${THIRD_PARTY}")

file(GLOB SRC src/*.cpp)
file(GLOB HRDS src/*.h)
find_package(Jemalloc REQUIRED)
find_package(GLOG REQUIRED)
find_package(GFLAGS REQUIRED)
find_package(PicoCoreDep REQUIRED)

set(CMAKE_CXX_COMPILER $ENV{CXX})
set(CMAKE_C_COMPILER $ENV{CC})

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
    #add_definitions(-O0 -g)
    add_definitions(-O3 -DNDEBUG)
endif()

set(CMAKE_SHARED_LINKER_FLAGS "-pthread -Wl,--whole-archive -lrt -Wl,--no-whole-archive")
set(CMAKE_EXE_LINKER_FLAGS "-pthread -Wl,--whole-archive -lrt -Wl,--no-whole-archive")

include_directories(SYSTEM src ${THIRD_PARTY}/include)
link_directories(${THIRD_PARTY}/lib ${THIRD_PARTY}/lib64)
set(CORE_LIB ${PicoCoreDep_LIBRARIES} )

set(MALLOC_DEFINITIONS -DUSE_JEMALLOC -DOVERRIDE_SYSTEM_MALLOC=0 -DOVERRIDE_SYSTEM_NEW=0)

add_library(pico_core_obj OBJECT ${SRC})

add_library(pico_core SHARED $<TARGET_OBJECTS:pico_core_obj>)
target_compile_definitions(pico_core PRIVATE ${MALLOC_DEFINITIONS})
target_link_libraries(pico_core PUBLIC ${Jemalloc_pic_LIBRARIES} ${RDMA_LIBRARIES} ${CORE_LIB} dl)
add_dependencies(pico_core pico_core_obj)


add_library(pico_core_static STATIC  $<TARGET_OBJECTS:pico_core_obj>)
target_compile_definitions(pico_core_static PRIVATE ${MALLOC_DEFINITIONS})
target_link_libraries(pico_core_static PUBLIC ${Jemalloc_pic_LIBRARIES} ${RDMA_LIBRARIES} ${CORE_LIB} dl)
add_dependencies(pico_core_static pico_core_obj)


add_executable(masterd src/masterd.cc)
target_link_libraries(masterd pico_core_static)

add_subdirectory(test)



INSTALL(TARGETS masterd pico_core_static pico_core
       RUNTIME DESTINATION bin
       LIBRARY DESTINATION lib
       ARCHIVE DESTINATION lib
)

INSTALL(DIRECTORY src/ DESTINATION include/pico-core
        FILES_MATCHING PATTERN "*.h")
