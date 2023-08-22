cmake_minimum_required(VERSION 3.5.1)

message(STATUS "CMAKE version ${CMAKE_VERSION}")
project(V2I)
set(CMAKE_CXX_STANDARD 14)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_CXX_FLAGS "-std=c++11 -g3 -rdynamic")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
# find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)
# find_package(Ceres REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CERES_INCLUDE_DIRS})
include_directories(${EIGEN3_INCLUDE_DIR})

# set the default modules to be compiled
set(compile_dirs common)
# compile only the necessary modules
list(APPEND compile_dirs civmap modules view)

foreach(subdir ${compile_dirs})
  if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/${subdir}")
    add_subdirectory(${subdir})
  endif()
endforeach()

# set(target_name V2I) add_executable(${target_name} main.cpp)
# target_link_libraries( ${target_name} PRIVATE ${PROJECT_NAME}_hmm
# ${OpenCV_LIBS} )
