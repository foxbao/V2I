cmake_minimum_required(VERSION 3.5)
project(KM)
set(CMAKE_CXX_STANDARD 11)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug")
endif()

find_package(GTest REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(GLOG REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Protobuf REQUIRED)

include(cmake/options.cmake)
include(cmake/functions.cmake)
include(cmake/process_proto.cmake)

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${EIGEN3_INCLUDE_DIR})


include_directories(core/src)
add_subdirectory(core)

# include_directories(middle_ware/cyber)
# add_subdirectory(middle_ware/cyber)

include_directories(common)
add_subdirectory(common)

if (CMAKE_SERVICE_TARGET)
	SET(SERVER_LIBRARIES 
		utils 
		webcore
		libzmq.so 
	)
	include_directories(fusion-service/inc)
	include_directories(../zsfd/inc)
	
	add_executable(fusion_service fusion-service/main.cpp)
	add_subdirectory(fusion-service)
	TARGET_LINK_LIBRARIES(fusion_service ${PROJECT_NAME}_common ${PROJECT_NAME}_core ${Protobuf_LIBRARIES} ${INNER_PROTO_TARGET_NAME} ${SERVER_LIBRARIES})	
else()
	add_executable(tests tests.cpp )
	TARGET_LINK_LIBRARIES(tests ${PROJECT_NAME}_common ${PROJECT_NAME}_core ${Protobuf_LIBRARIES} ${INNER_PROTO_TARGET_NAME})	
endif()


