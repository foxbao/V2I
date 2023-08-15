set(MODULE_NAME ${PROJECT_NAME}_common)

FILE( GLOB CPPS [^.]*.cpp [^.]*.cc)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)

include(../cmake/functions.cmake)

add_library(${MODULE_NAME} SHARED)
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


target_include_directories(${MODULE_NAME}
PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${PROJECT_SOURCE_DIR}
)





set(target_name fit_test)
add_executable(${target_name} fit_test.cpp)
target_link_libraries(
	${target_name}
	PRIVATE
	${MODULE_NAME}
	${CERES_LIBRARIES}
	${OpenCV_LIBS}
	)

