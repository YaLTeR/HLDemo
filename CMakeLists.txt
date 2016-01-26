cmake_minimum_required (VERSION 3.1)
project (HLDemo)

if (NOT MSVC)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -march=native -mtune=native -Ofast -Wall -Wextra")
endif ()

set (LIBRARY_OUTPUT_DIRECTORY ".")
set (SOURCE_FILES
	src/DemoFile.cpp
)
set (HEADER_FILES
	src/DemoFile.hpp
	src/DemoFrame.hpp
)

if (MSVC)
	set (SOURCE_FILES ${SOURCE_FILES} ${HEADER_FILES})
endif ()

add_library (HLDemo ${SOURCE_FILES})
