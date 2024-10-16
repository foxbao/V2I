cmake_minimum_required(VERSION 3.5)
project(mec)
 
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

SET(CMAKE_SERVICE_TARGET "SERVICE")
 
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)   #导出clangd需要的文件，用于智能提示和基于语议的补全
 
set(CMAKE_CXX_FLAGS "-std=c++11 -g3 -rdynamic")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

add_definitions(-DUSED_BY_MEC)
 
#打印信息
message(STATUS "build type:")
message(STATUS "CXX_FLAGS = ${CMAKE_CXX_FLAGS} ")
 
#设置库文件输出目录
if (NOT LIBRARY_OUTPUT_PATH) 
    set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/libs)
endif()

if (NOT EXECUTABLE_OUTPUT_PATH)
    set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/bin)
endif()

if (NOT EXTERNAL_LIB_ROOT_PATH)
    set(EXTERNAL_LIB_ROOT_PATH "~/zassys/x86")
endif()

link_directories(
    ${LIBRARY_OUTPUT_PATH}
)

if (NOT CMAKE_BUILD_MODULE)
	add_subdirectory(zsfd)
	add_subdirectory(tools/osm-parser1/odrviewer)
	add_subdirectory(mapcore)
	add_subdirectory(jni)
	add_subdirectory(tools/osm-parser1)
	add_subdirectory(test/mapcore)
	add_subdirectory(vehicle-snapshot-service)
	add_subdirectory(vehicle-indexing)
	add_subdirectory(load-balance)
	add_subdirectory(fusion-service)
elseif (CMAKE_BUILD_MODULE STREQUAL "map") 
	add_subdirectory(zsfd/utils)
	add_subdirectory(tools/osm-parser1/odrviewer)
	add_subdirectory(mapcore)
	add_subdirectory(jni)
	add_subdirectory(tools/osm-parser1)
	add_subdirectory(test/mapcore)
elseif (CMAKE_BUILD_MODULE STREQUAL "mapcore") 
	add_subdirectory(zsfd/utils)
	add_subdirectory(tools/osm-parser1/odrviewer)
	add_subdirectory(mapcore)
	add_subdirectory(jni)
	add_subdirectory(test/mapcore)
elseif (CMAKE_BUILD_MODULE STREQUAL "service") 
	add_subdirectory(zsfd)
	add_subdirectory(vehicle-snapshot-service)
	add_subdirectory(vehicle-indexing)
	add_subdirectory(load-balance)
	add_subdirectory(fusion-service)
elseif (CMAKE_BUILD_MODULE STREQUAL "zsfd") 
	add_subdirectory(zsfd)
endif()


INSTALL(DIRECTORY ${LIBRARY_OUTPUT_PATH}/ DESTINATION /zassys/lib/x86_64-linux-gnu)

if (NOT (CMAKE_BUILD_MODULE STREQUAL "map") AND NOT (CMAKE_BUILD_MODULE STREQUAL "mapcore"))
	INSTALL(PROGRAMS 
		${EXECUTABLE_OUTPUT_PATH}/zlaunch 
		${EXECUTABLE_OUTPUT_PATH}/zhost 
		DESTINATION /zassys/bin)

	INSTALL(PROGRAMS 
		${EXECUTABLE_OUTPUT_PATH}/rpcbridge 
		${EXECUTABLE_OUTPUT_PATH}/zsysd 
		DESTINATION /zassys/sysapp/zas.system/daemons)

	INSTALL(DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/load_balance
	DESTINATION /zassys/sysapp/others/load-balance)

	INSTALL(DIRECTORY ${PROJECT_SOURCE_DIR}/load-balance/config/ 
	DESTINATION /zassys/sysapp/others/load-balance/config)

	INSTALL(DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/fusion_service
	DESTINATION /zassys/sysapp/others/fusion-service)

	INSTALL(DIRECTORY ${PROJECT_SOURCE_DIR}/fusion-service/fusion-service/config/ 
	DESTINATION /zassys/sysapp/others/fusion-service/config)

	INSTALL(DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/indexing_service
	DESTINATION /zassys/sysapp/others/vehicle-indexing)

	INSTALL(DIRECTORY ${PROJECT_SOURCE_DIR}/vehicle-indexing/config/ 
	DESTINATION /zassys/sysapp/others/vehicle-indexing/config)

	INSTALL(DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/snapshot_service
	DESTINATION /zassys/sysapp/others/vehicle-snapshot-service)

	INSTALL(DIRECTORY ${PROJECT_SOURCE_DIR}/vehicle-snapshot-service/config/ 
	DESTINATION /zassys/sysapp/others/vehicle-snapshot-service/config)
	
endif()

#cmake中执行shell指令
execute_process(COMMAND pwd)