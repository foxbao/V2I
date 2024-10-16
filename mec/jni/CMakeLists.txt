cmake_minimum_required(VERSION 3.5)
project(hdmapjni)
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


FILE( GLOB CPPS [^.]*.cpp [^.]*.cc)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)

add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME}
PRIVATE
${HPPS}
${CPPS}
)

# set the include path
include_directories(${PROJECT_NAME}
PUBLIC
${CMAKE_CURRENT_LIST_DIR}
${CMAKE_CURRENT_LIST_DIR}/../zsfd/inc
${CMAKE_CURRENT_LIST_DIR}/../tools/osm-parser1/odrviewer
)

SET(LIBRARIES 
utils
OpenDrive
mapcore
)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${LIBRARIES})

