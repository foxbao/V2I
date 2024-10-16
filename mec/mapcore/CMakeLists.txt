cmake_minimum_required(VERSION 3.5)
project(mapcore)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

add_definitions("-DLIBMAPCORE")

if (NOT LIBRARY_OUTPUT_PATH) 
    set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/libs)
endif()

if (NOT EXECUTABLE_OUTPUT_PATH)
    set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/bin)
endif()

if (NOT EXTERNAL_LIB_ROOT_PATH)
    set(EXTERNAL_LIB_ROOT_PATH "/home/coder/zassys/x86")
endif()

find_package(Eigen3 REQUIRED)
find_package(PROJ CONFIG REQUIRED)

FILE( GLOB CPPS [^.]*.cpp [^.]*.cc)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)

add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME}
PRIVATE
${HPPS}
${CPPS}
)


target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../zsfd/inc
    ${CMAKE_CURRENT_LIST_DIR}/../zsfd/hwgrph
    ${CMAKE_CURRENT_LIST_DIR}/../tools/osm-parser1/odrviewer
    ${CMAKE_CURRENT_LIST_DIR}/../tools/osm-parser1/odrviewer/Thirdparty
    ${EIGEN3_INCLUDE_DIR}
)
# set the include path
# include_directories(${PROJECT_NAME}
# include_directories(${EIGEN3_INCLUDE_DIR})
# PUBLIC
# ${CMAKE_CURRENT_LIST_DIR}
# ${CMAKE_CURRENT_LIST_DIR}/../zsfd/inc
# ${CMAKE_CURRENT_LIST_DIR}/../zsfd/hwgrph
# ${CMAKE_CURRENT_LIST_DIR}/../tools/osm-parser1/odrviewer
# ${CMAKE_CURRENT_LIST_DIR}/../tools/osm-parser1/odrviewer/Thirdparty
# )

SET(LIBRARIES 
utils
OpenDrive
)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${LIBRARIES} PROJ::proj)

