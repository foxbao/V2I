project(modules)
set(MODULE_NAME ${PROJECT_NAME})
add_library(${MODULE_NAME} SHARED)

add_subdirectory(Gap)
add_subdirectory(HMM)
add_subdirectory(TTC)
add_subdirectory(Trajectory)

file(GLOB CPPS [^.]*.cpp [^.]*.cc)
file(GLOB HPPS [^.]*.hpp [^.]*.h)

target_sources(${MODULE_NAME} PRIVATE ${HPPS} ${CPPS})

target_include_directories(${MODULE_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/
                                                 ${CMAKE_SOURCE_DIR}
                                                 ${CMAKE_SOURCE_DIR}/mec/tools/osm-parser1/odrviewer
                                                 ${CMAKE_SOURCE_DIR}/mec/zsfd/inc)

target_link_libraries(
  ${MODULE_NAME}
  PUBLIC
  PRIVATE common civmap mapcore OpenDrive utils)

set(target_name ${MODULE_NAME}_test)
add_executable(${target_name} modules_test.cpp)
target_link_libraries(${target_name} PRIVATE ${MODULE_NAME} Geographiccc common)
