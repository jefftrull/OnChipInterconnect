# Build control file for "Analyzing On-Chip Interconnect with Modern C++" examples

cmake_minimum_required(VERSION 2.8)

find_package( Boost 1.56 REQUIRED )
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
if (MSVC)
  # prevent auto linking
  add_definitions( -DBOOST_ALL_NO_LIB=1 )
endif()

if (CMAKE_COMPILER_IS_GNUCC OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
  add_definitions(-Wall -Wextra -Werror)  # be conservative about checks
  add_definitions(-std=c++0x)             # enable C++11
endif()

add_executable(graph_analysis graph_analysis.cpp)
