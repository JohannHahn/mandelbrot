cmake_minimum_required(VERSION 3.12)

project(plot)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(raylib)
include_directories(raylib/src)

add_executable(plot plot.cpp)

if (MSVC)
    #target_compile_options(MyApp PRIVATE /std:c++latest)
    #target_link_libraries(plot raylib stdc++latest)
    target_compile_options(plot PRIVATE /std:c++latest)
    target_link_libraries(plot raylib)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(plot raylib stdc++exp)
endif()
#target_link_libraries(plot raylib stdc++exp)

