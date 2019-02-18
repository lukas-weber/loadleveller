cmake_minimum_required(VERSION 3.1)
project(load_leveller)

find_package (PkgConfig REQUIRED)
find_package(MPI REQUIRED)
find_package(fmt CONFIG REQUIRED)
pkg_check_modules(yaml-cpp REQUIRED IMPORTED_TARGET yaml-cpp)
pkg_check_modules(HDF5 REQUIRED IMPORTED_TARGET HDF5)

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
target_link_libraries(load_leveller PUBLIC MPI::MPI_CXX fmt::fmt PkgConfig::yaml-cpp PkgConfig::HDF5)
install(FILES load_leveller.h DESTINATION include)
