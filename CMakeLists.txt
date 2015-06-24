cmake_minimum_required(VERSION 2.8)
project(RPLLoader)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

add_subdirectory(externals/zlib)
include_directories(externals/zlib)

add_executable(rplloader rplloader.cpp)
target_link_libraries(rplloader z)
