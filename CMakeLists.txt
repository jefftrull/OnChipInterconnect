# Build control file for "Analyzing On-Chip Interconnect with Modern C++" examples

cmake_minimum_required(VERSION 2.8)

find_package( Boost 1.61 REQUIRED COMPONENTS context )
if (MSVC)
  # prevent auto linking
  add_definitions( -DBOOST_ALL_NO_LIB=1 )
endif()

if (CMAKE_COMPILER_IS_GNUCC OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
  add_definitions(-Wall -Wextra -Werror)  # be conservative about checks
  add_definitions(-std=c++11)             # enable C++11
endif()

foreach( excpp resistive_loops floating_nodes rmst graph_elmore spef_rline_parse spef_rline_parse_coro spef_parse units_udl )
  add_executable( ${excpp} ${excpp}.cpp )
  target_link_libraries( ${excpp} Boost::boost )
endforeach()

target_link_libraries( spef_rline_parse_coro Boost::boost ${Boost_LIBRARIES} )

# eigen-dependent code
find_package( Eigen3 )
if( EIGEN3_FOUND )
  include_directories( SYSTEM ${EIGEN3_INCLUDE_DIR} )
  include_directories( ${CMAKE_SOURCE_DIR} )
  if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Clang warns on some Eigen things so we need to do this:
    add_definitions( -Wno-sign-compare -Wno-deprecated-register -Wno-deprecated-declarations )
  endif()
  add_executable( matrix matrix.cpp )
  target_link_libraries( matrix Boost::boost )
else()
  message( "Eigen3 does not seem to be installed.  Skipping some examples" )
endif()
