cmake_minimum_required(VERSION 2.8.0)
project(mredis)
include_directories(./)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -g -pthread")

# common source files
set (SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/mredis/src/zmalloc.cc"
    )

# test source files 
set (TEST_SOURCE_FILES
     ""
    )

add_executable (mredis
  ${SOURCE_FILES})