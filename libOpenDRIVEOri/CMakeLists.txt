cmake_minimum_required(VERSION 3.10)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

project(libOpenDriveOri VERSION 0.6.0 DESCRIPTION ".xodr library")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(MSVC)
    set(CMAKE_CXX_FLAGS "/EHsc /Wall")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2")
else()
    set(CMAKE_CXX_FLAGS "-Wall")
    set(CMAKE_CXX_FLAGS_DEBUG "-g")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif()

set(SOURCES 
    src/Geometries/Arc.cpp
    src/Geometries/CubicSpline.cpp
    src/Geometries/Line.cpp
    src/Geometries/ParamPoly3.cpp
    src/Geometries/RoadGeometry.cpp
    src/Geometries/Spiral.cpp
    src/Geometries/Spiral/odrSpiral.cpp
    src/Junction.cpp
    src/Lane.cpp
    src/LaneSection.cpp
    src/Mesh.cpp
    src/OpenDriveMap.cpp
    src/RefLine.cpp
    src/Road.cpp
    src/RoadMark.cpp
    src/RoadNetworkMesh.cpp
    src/RoadObject.cpp
    src/RoadSignal.cpp
    src/RoutingGraph.cpp
    thirdparty/pugixml/pugixml.cpp
)

add_library(OpenDriveOri ${SOURCES})
target_include_directories(OpenDriveOri
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/thirdparty>)

add_executable(test-xodrOri test.cpp)
target_link_libraries(test-xodrOri OpenDriveOri)

install(
    TARGETS OpenDriveOri test-xodrOri
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION "include")
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/pugixml/pugixml.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/pugixml/pugiconfig.hpp DESTINATION "include/pugixml/")

install(TARGETS OpenDriveOri EXPORT OpenDriveConfig)
install(EXPORT OpenDriveConfig NAMESPACE OpenDrive:: DESTINATION cmake)
