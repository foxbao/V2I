cmake_minimum_required(VERSION 3.5)
project(load_balance)
set(CMAKE_CXX_STANDARD 11)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

message ("loadbalnce mode" ${CMAKE_BUILD_TYPE} ${PROJECT_NAME})

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include_directories(../zsfd/inc)

SET(LIBRARIES 
utils 
webcore
libzmq.so 
)

link_directories(
	${EXTERNAL_LIB_ROOT_PATH}/lib
)

add_executable(${PROJECT_NAME} main.cpp)

include_directories(inc)
add_subdirectory(src)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${LIBRARIES})	




