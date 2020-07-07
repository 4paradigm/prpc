cmake_minimum_required(VERSION 3.10)

if(NOT THIRD_PARTY)
    set(THIRD_PARTY "third-party")
endif()
message(STATUS "third party: ${THIRD_PARTY}")

option(SKIP_BUILD_TEST "whether skip build testcase" OFF)

file(GLOB SRC src/*.cpp src/observability/metrics/*.cpp)
file(GLOB HRDS src/*.h src/observability/metrics/*.h)
find_package(Jemalloc REQUIRED)
find_package(PicoCoreDep REQUIRED)

if (EXISTS $ENV{CXX})
    set(CMAKE_CXX_COMPILER $ENV{CXX})
else()
    execute_process(COMMAND which g++ OUTPUT_VARIABLE CMAKE_CXX_COMPILER)
    string(STRIP ${CMAKE_CXX_COMPILER} CMAKE_CXX_COMPILER)
endif()
if (EXISTS $ENV{CC})
    set(CMAKE_C_COMPILER $ENV{CC})
else()
    execute_process(COMMAND which gcc OUTPUT_VARIABLE CMAKE_C_COMPILER)
    string(STRIP ${CMAKE_C_COMPILER} CMAKE_C_COMPILER)
endif()

# check gcc version
if(CMAKE_COMPILER_IS_GNUCXX)
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
    if(GCC_VERSION VERSION_GREATER 5.2.0 OR GCC_VERSION VERSION_EQUAL 5.2.0)
        message(STATUS "C++14 activated.")
    else()
        message(FATAL_ERROR "C++14 needed. Therefore a gcc compiler with a version higher than 5.2 is needed.")
    endif()
else(CMAKE_COMPILER_IS_GNUCXX)
    message(FATAL_ERROR "only gcc supported")
endif(CMAKE_COMPILER_IS_GNUCXX)

# !!!严禁去掉-Werror，没有任何例外!!!
add_definitions(--std=c++14 -Wall -Wextra -Wno-deprecated-declarations -Werror -frecord-gcc-switches -fPIC)

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
set(CORE_STATIC_LIB ${PicoCoreDep_STATIC_LIBRARIES} )

set(MALLOC_DEFINITIONS -DUSE_JEMALLOC -DOVERRIDE_SYSTEM_MALLOC=0 -DOVERRIDE_SYSTEM_NEW=0)

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


add_executable(masterd src/masterd.cc)
target_link_libraries(masterd pico_core_static)

if (not SKIP_BUILD_TEST)
    add_subdirectory(test)
endif()



INSTALL(TARGETS masterd pico_core_static pico_core
       RUNTIME DESTINATION bin
       LIBRARY DESTINATION lib
       ARCHIVE DESTINATION lib
)

INSTALL(DIRECTORY src/ DESTINATION include/pico-core
        FILES_MATCHING PATTERN "*.h")
