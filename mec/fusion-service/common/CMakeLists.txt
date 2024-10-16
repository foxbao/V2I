
set(MODULE_NAME ${PROJECT_NAME}_common)
add_library(${MODULE_NAME} SHARED)

add_subdirectory(util)

FILE( GLOB CPPS [^.]*.cpp [^.]*.cc)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)


include_directories(${Protobuf_INCLUDE_DIRS})
target_sources(${MODULE_NAME}
PUBLIC
${HPPS}
${CPPS}
)

target_include_directories(${MODULE_NAME}
PUBLIC
${CMAKE_CURRENT_LIST_DIR}
${CMAKE_CURRENT_LIST_DIR}
)


TARGET_LINK_LIBRARIES(${MODULE_NAME}  glog::glog ${Protobuf_LIBRARIES})