cmake_minimum_required(VERSION 2.8.0)
project(mredis)
include_directories(./)
include_directories(./thirdparty/include) 

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -g -pthread")

# common source files
set (SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/mredis/src/zmalloc.cc"
    "${CMAKE_SOURCE_DIR}/mredis/src/string.cc"
    "${CMAKE_SOURCE_DIR}/mredis/src/intset.cc"
    )

# test source files 
set (TEST_SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/mredis/test/test_main.cc"
    "${CMAKE_SOURCE_DIR}/mredis/test/string_test.cc"
    "${CMAKE_SOURCE_DIR}/mredis/test/dict_test.cc"
    "${CMAKE_SOURCE_DIR}/mredis/test/skiplist_test.cc"
    "${CMAKE_SOURCE_DIR}/mredis/test/intset_test.cc"
    )

add_executable (mredistest
  ${SOURCE_FILES}
  ${TEST_SOURCE_FILES})

# link thirdparty libs
set (THIRDPARTY_DIR ${CMAKE_SOURCE_DIR}/thirdparty)

target_link_libraries (mredistest
    "${THIRDPARTY_DIR}/lib/gtest/libgtest.a"
    "${THIRDPARTY_DIR}/lib/glog/libglog.a"
    )