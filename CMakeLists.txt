cmake_minimum_required(VERSION 3.10)

project(pico-core)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

if (THIRD_PARTY)
    set(CMAKE_PREFIX_PATH "${THIRD_PARTY}")
    message(STATUS "THIRD_PARTY=${THIRD_PARTY}")
    include_directories(SYSTEM ${THIRD_PARTY}/include)
    link_directories(${THIRD_PARTY}/lib ${THIRD_PARTY}/lib64)
endif()

option(SKIP_BUILD_TEST "whether skip build testcase" OFF)
message(STATUS "SKIP_BUILD_TEST: ${SKIP_BUILD_TEST}")

# check gcc version
if(CMAKE_COMPILER_IS_GNUCXX)
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
    message(STATUS "gcc ${GCC_VERSION}")
    if(GCC_VERSION VERSION_GREATER 5.2.0 OR GCC_VERSION VERSION_EQUAL 5.2.0)
        message(STATUS "C++14 activated.")
    else()
        message(FATAL_ERROR "C++14 needed. Therefore a gcc compiler with a version higher than 5.2 is needed.")
    endif()
else()
    message(FATAL_ERROR "only gcc supported")
endif()

# !!!严禁去掉-Werror，没有任何例外!!!
add_definitions(--std=c++14 -Wall -Wextra -Wno-deprecated-declarations -Werror -frecord-gcc-switches -fPIC)
if (DEBUG)
    add_definitions(-O0 -g)
else()
    add_definitions(-O3 -DNDEBUG)
endif()

set(CMAKE_SHARED_LINKER_FLAGS "-pthread -Wl,--whole-archive -lrt -Wl,--no-whole-archive")
set(CMAKE_EXE_LINKER_FLAGS "-pthread -Wl,--whole-archive -lrt -Wl,--no-whole-archive")

option(USE_RDMA "whether build with rdma support" OFF)
if (USE_RDMA)
    add_definitions(-DUSE_RDMA)
    set(RDMA_LIBRARIES rdmacm ibverbs)
    message(STATUS "RDMA enabled")
else()
    message(STATUS "RDMA disabled")
    set(RDMA_LIBRARIES )
endif()

find_package(Jemalloc REQUIRED)
find_package(PicoCoreDep REQUIRED)

include_directories(BEFORE SYSTEM src/accumulator src/addition src/codec src/common src/misc src/rpc src/thread)
file(GLOB SRC src/accumulator/*.cpp src/addition/*.cpp src/codec/*.cpp src/common/*.cpp src/misc/*.cpp src/misc/observability/metrics/*.cpp  src/rpc/*.cpp src/thread/*.cpp)

set(CORE_LIB ${PicoCoreDep_LIBRARIES} )
set(CORE_STATIC_LIB ${PicoCoreDep_STATIC_LIBRARIES} )

set(MALLOC_DEFINITIONS -DUSE_JEMALLOC -DJEMALLOC_NO_RENAME -DOVERRIDE_SYSTEM_MALLOC=0 -DOVERRIDE_SYSTEM_NEW=0)
set(CORE_LIB ${PicoCoreDep_LIBRARIES} )
set(CORE_STATIC_LIB ${PicoCoreDep_STATIC_LIBRARIES} )

add_library(pico_core_obj OBJECT ${SRC})
target_compile_definitions(pico_core_obj PRIVATE ${MALLOC_DEFINITIONS})

add_library(pico_core SHARED $<TARGET_OBJECTS:pico_core_obj>)
target_compile_definitions(pico_core PRIVATE ${MALLOC_DEFINITIONS})
target_link_libraries(pico_core PUBLIC ${Jemalloc_pic_LIBRARIES} ${RDMA_LIBRARIES} ${CORE_LIB} dl)
add_dependencies(pico_core pico_core_obj)


add_library(pico_core_static STATIC  $<TARGET_OBJECTS:pico_core_obj>)
target_compile_definitions(pico_core_static PRIVATE ${MALLOC_DEFINITIONS})
target_link_libraries(pico_core_static PUBLIC ${Jemalloc_pic_STATIC_LIBRARIES} ${RDMA_LIBRARIES} ${CORE_STATIC_LIB} dl)
add_dependencies(pico_core_static pico_core_obj)


add_executable(masterd src/rpc/masterd.cc)
target_link_libraries(masterd pico_core_static)

if (NOT SKIP_BUILD_TEST)
    enable_testing()
endif()
add_subdirectory(test)


INSTALL(TARGETS masterd pico_core_static pico_core
       RUNTIME DESTINATION bin
       LIBRARY DESTINATION lib
       ARCHIVE DESTINATION lib
)

INSTALL(DIRECTORY src/accumulator/ src/addition/ src/codec/ src/common/ src/misc/ src/rpc/ src/thread/
        DESTINATION include/pico-core FILES_MATCHING PATTERN "*.h")

