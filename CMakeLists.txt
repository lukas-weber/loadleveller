cmake_minimum_required(VERSION 2.6)
project(load_leveller)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(MPI REQUIRED)
set(CMAKE_CXX_COMPILER ${MPI_CXX_COMPILER})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -O3 -std=c++17 -pedantic ${MPI_LINK_FLAGS} ${MPI_COMPILE_FLAGS}")
include_directories(${MPI_INCLUDE_PATH})

option(MEASUREMENTS_APPEND "Use append mode for writing measurements" OFF)
option(DUMP_BUFFER "dump uses buffered writeout" OFF)

if(MEASUREMENTS_APPEND)
	add_definitions(-DMCL_MEASUREMENTS_APPEND=1)
endif()
if(DUMP_BUFFER)
	add_definitions(-DMCL_DUMP_BUFFER=1)
endif()

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

add_library(load_leveller STATIC ${SRCs})
target_link_libraries(load_leveller ${MPI_LIBRARIES})


SET(MCPREFIX "$ENV{HOME}/mc/" CACHE PATH "Prefix prepended to
install directories")
SET(CMAKE_INSTALL_PREFIX "${MCPREFIX}" CACHE INTERNAL "Prefix
prepended to install directories" FORCE)

install(TARGETS load_leveller DESTINATION .)
