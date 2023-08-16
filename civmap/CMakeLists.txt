set(MODULE_NAME ${PROJECT_NAME}_map)
add_library(${MODULE_NAME} SHARED)
FILE( GLOB CPPS [^.]*.cpp [^.]*.cc)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)

target_sources(${MODULE_NAME}
PRIVATE
    ${HPPS}
    ${CPPS}
)

target_include_directories(${MODULE_NAME}
PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/
	${CMAKE_SOURCE_DIR}
)

target_link_libraries(
	${MODULE_NAME}
	PRIVATE
	V2I_common
	Geographiccc
)

set(target_name ${MODULE_NAME}_test)
add_executable(${target_name} map_test.cpp)
target_link_libraries(
	${target_name}
	PRIVATE
	${MODULE_NAME}
	)
