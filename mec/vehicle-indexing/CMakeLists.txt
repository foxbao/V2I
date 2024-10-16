cmake_minimum_required(VERSION 3.5)
project(indexing_service)
set(CMAKE_CXX_STANDARD 11)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

message ("loadbalnce mode" ${CMAKE_BUILD_TYPE} ${PROJECT_NAME})

include(../cmake/options.cmake)
include(../cmake/functions.cmake)
include(../cmake/process_proto.cmake)

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include_directories(../zsfd/inc)

SET(LIBRARIES 
utils 
webcore
mapcore
libzmq.so 
librabbitmq.so
)

link_directories(
	${EXTERNAL_LIB_ROOT_PATH}/lib
)

add_executable(${PROJECT_NAME} main.cpp)

include_directories(inc)
add_subdirectory(src)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${Protobuf_LIBRARIES} ${INNER_PROTO_TARGET_NAME} ${LIBRARIES})	




