cmake_minimum_required(VERSION 3.12)


project(plot)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


add_subdirectory(libs/raylib)
include_directories(libs/raylib/src)
include_directories(libs/gmp/include)

add_executable(plot plot.cpp)

target_link_directories(plot PRIVATE libs/gmp/lib)

#set(CMAKE_BUILD_TYPE RelWithDebInfo)



if (MSVC)
    #target_compile_options(MyApp PRIVATE /std:c++latest)
    #target_link_libraries(plot raylib stdc++latest)
    target_compile_options(plot PRIVATE /std:c++latest)
    target_link_libraries(plot raylib gmp mpfr)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(plot raylib stdc++exp gmp mpfr)
endif()
#target_link_libraries(plot raylib stdc++exp)

