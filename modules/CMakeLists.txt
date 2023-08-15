set(MODULE_NAME ${PROJECT_NAME}_modules)
add_library(${MODULE_NAME} SHARED)


add_subdirectory(Gap)
add_subdirectory(HMM)
add_subdirectory(TTC)


target_include_directories(${MODULE_NAME}
PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/
    ${CMAKE_SOURCE_DIR}
)


set(target_name ${MODULE_NAME}_test)
add_executable(${target_name} test.cpp)
target_link_libraries(
	${target_name}
	PRIVATE
	${MODULE_NAME}
	)

