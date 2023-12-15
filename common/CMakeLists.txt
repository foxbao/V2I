project(common)
cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 14)

if(TARGET_VENDER STREQUAL MDC)
    message("---mdc not build zloc/view----")
elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(BUILD_VIEW TRUE)
    add_definitions(-D__BUILD_VIEW_)
    set(BUILD_FILESYSTEM TRUE)
    add_definitions(-D__BUILD_FILESYSTEM_)
    set(BUILD_TOOL TRUE)
endif()

FILE( GLOB CPPS [^.]*.cpp [^.]*.cc)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)

include(../cmake/functions.cmake)

set( MODULE_NAME ${PROJECT_NAME})

add_library(${MODULE_NAME} SHARED)

target_include_directories(${MODULE_NAME}
PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${PROJECT_SOURCE_DIR}
)
target_sources(${MODULE_NAME}
PRIVATE
    ${HPPS}
    ${CPPS}
)

SUBDIRLIST(dir_list ${CMAKE_CURRENT_SOURCE_DIR})
foreach(subdir ${dir_list})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/CMakeLists.txt)
        add_subdirectory(${subdir})
    endif()
endforeach()


if(BUILD_FILESYSTEM)
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
endif()


# set(target_name fit_test)
# add_executable(${target_name} fit_test.cpp)
# target_link_libraries(
# 	${target_name}
# 	PRIVATE
# 	${MODULE_NAME}
# 	${CERES_LIBRARIES}
# 	${OpenCV_LIBS}
# 	)

