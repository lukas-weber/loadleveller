cmake_minimum_required(VERSION 3.1)
project(load_leveller)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(MPI REQUIRED)
find_package(fmt REQUIRED)
find_package(HDF5 REQUIRED)

set(CMAKE_CXX_STANDARD 17)

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
	runner_task.cpp
	results.cpp
)

add_library(load_leveller ${SRCs})
target_link_libraries(load_leveller PUBLIC MPI::MPI_CXX fmt::fmt)
install(FILES load_leveller.h DESTINATION include)
