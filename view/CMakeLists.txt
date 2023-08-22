
project(civview)
set(MODULE_NAME ${PROJECT_NAME})
find_package(OpenCV REQUIRED)
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
	${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/..
)

TARGET_LINK_LIBRARIES(${MODULE_NAME} 
PUBLIC
    ${OpenCV_LIBS} 
PRIVATE
    civmap
    common
    modules
)

set(target_name ${MODULE_NAME}_test)
add_executable(${target_name} imgprocessor_test.cpp)
target_link_libraries(
	${target_name}
	PRIVATE
	${MODULE_NAME}
    common
    modules
)



