cmake_minimum_required(VERSION 3.5)
project(snapshot_service)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

message ("loadbalnce mode" ${CMAKE_BUILD_TYPE} ${PROJECT_NAME})

find_package(Eigen3 REQUIRED)

include(../cmake/options.cmake)
include(../cmake/functions.cmake)
include(../cmake/process_proto.cmake)

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${EIGEN3_INCLUDE_DIR})

include_directories(${PROJECT_NAME}
PUBLIC
${CMAKE_CURRENT_LIST_DIR}
${CMAKE_CURRENT_LIST_DIR}/../zsfd/inc
${CMAKE_CURRENT_LIST_DIR}/../zsfd/hwgrph
${CMAKE_CURRENT_LIST_DIR}/../tools/osm-parser1/odrviewer
${CMAKE_CURRENT_LIST_DIR}/../tools/osm-parser1/odrviewer/Thirdparty
${CMAKE_CURRENT_LIST_DIR}/../fusion-service
)

SET(LIBRARIES 
utils 
webcore
mapcore
KM_core
KM_common
libzmq.so 
librabbitmq.so
librdkafka++.so
librabbitmq.so
)

link_directories(
	${EXTERNAL_LIB_ROOT_PATH}/lib
)

add_executable(${PROJECT_NAME} main.cpp)

include_directories(inc)
add_subdirectory(src)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${Protobuf_LIBRARIES} ${INNER_PROTO_TARGET_NAME} ${LIBRARIES})