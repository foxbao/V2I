project(civview)
set(MODULE_NAME ${PROJECT_NAME})
find_package(OpenCV REQUIRED)
add_library(${MODULE_NAME} SHARED)

file(GLOB CPPS [^.]*.cpp [^.]*.cc)
file(GLOB HPPS [^.]*.hpp [^.]*.h)

target_sources(${MODULE_NAME} PRIVATE ${HPPS} ${CPPS})
target_include_directories(${MODULE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}
                                                 ${CMAKE_SOURCE_DIR}/..)

target_link_libraries(
  ${MODULE_NAME}
  PUBLIC ${OpenCV_LIBS}
  PRIVATE civmap common modules)

set(target_name ${MODULE_NAME}_test)
add_executable(${target_name} imgprocessor_test.cpp)
target_link_libraries(${target_name} 
  PRIVATE ${MODULE_NAME} mapcore civmap common modules)
