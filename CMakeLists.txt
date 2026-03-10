cmake_minimum_required(VERSION 3.28)

project("qprof" C CXX)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${qprof_SOURCE_DIR}/cmake")

find_package(QProf REQUIRED)

add_library(
    qprof STATIC
    Profiler.cpp
    Profiler.hpp
)
add_executable(qprof_example main.cpp)
target_link_libraries(qprof_example PRIVATE qprof QProf::QProf)
target_include_directories(qprof PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qprof PRIVATE QProf::QProf)

set_target_properties(qprof PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)