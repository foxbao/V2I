cmake_minimum_required(VERSION 3.5)
project(zsfd)
 
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

 
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)   #导出clangd需要的文件，用于智能提示和基于语议的补全
 
set(CMAKE_CXX_FLAGS "-std=c++11 -g3 -rdynamic")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
 
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

add_subdirectory(utils)
add_subdirectory(mware)
add_subdirectory(webcore)
add_subdirectory(sys/coreapp/sysd)
add_subdirectory(sys/coreapp/rpcbridge)
add_subdirectory(sys/host)
add_subdirectory(sys/launch)

#cmake中执行shell指令
execute_process(COMMAND pwd)