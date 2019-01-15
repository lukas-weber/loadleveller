cmake_minimum_required(VERSION 2.6)
project(load_leveller)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(MPI REQUIRED)
set(CMAKE_CXX_COMPILER ${MPI_CXX_COMPILER})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -O3 -std=c++17 -pedantic ${MPI_LINK_FLAGS} ${MPI_COMPILE_FLAGS}")
include_directories(${MPI_INCLUDE_PATH})

set(HDF5_USE_STATIC_LIBRARIES)
find_package(HDF5 REQUIRED)
include_directories(${HDF5_INCLUDE_DIR})
add_definitions(${HDF5_C_DEFINITIONS})

set(SRCs
	dump.cpp
	evalable.cpp
	measurements.cpp 
	observable.cpp
	parser.cpp
	random.cpp
	runner.cpp
	merger.cpp
	mc.cpp
	runner_single.cpp
)

add_library(load_leveller ${SRCs})
add_library(load_leveller STATIC ${SRCs})
#target_link_libraries(load_leveller ${MPI_LIBRARIES} ${CONAN_LIBS})
