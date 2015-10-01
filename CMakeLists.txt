cmake_minimum_required (VERSION 3.1)
project (HLDemo)

if (NOT MSVC)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -march=native -mtune=native -Ofast -Wall -Wextra")
endif ()

find_package (Boost 1.57.0 REQUIRED)
include_directories (${Boost_INCLUDE_DIR})

set (LIBRARY_OUTPUT_DIRECTORY ".")
set (SOURCE_FILES
	src/DemoFile.cpp
)

add_library (HLDemo ${SOURCE_FILES})
