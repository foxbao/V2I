cmake_minimum_required(VERSION 3.5)
project(mapcore_test)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

if (NOT LIBRARY_OUTPUT_PATH) 
    set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/libs)
endif()

if (NOT EXECUTABLE_OUTPUT_PATH)
    set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/bin)
endif()

if (NOT EXTERNAL_LIB_ROOT_PATH)
    set(EXTERNAL_LIB_ROOT_PATH "/home/coder/zassys/x86")
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include_directories(../../zsfd/inc)
include_directories(../../tools/osm-parser1/odrviewer)

SET(LIBRARIES 
utils
mapcore
)

link_directories(
	${LIBRARY_OUTPUT_PATH}
	${EXTERNAL_LIB_ROOT_PATH}/lib
)

add_executable(${PROJECT_NAME} main.cpp)


TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${LIBRARIES})	




