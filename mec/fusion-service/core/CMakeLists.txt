set(MODULE_NAME ${PROJECT_NAME}_core)

add_library(${MODULE_NAME} SHARED)
add_subdirectory(src)

FILE( GLOB CPPS [^.]*.cpp [^.]*.cc)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)

target_sources(${MODULE_NAME}
PUBLIC
${HPPS}
${CPPS}
)
# set the include path
target_include_directories(${MODULE_NAME}
PUBLIC
${CMAKE_CURRENT_LIST_DIR}/
${CMAKE_CURRENT_LIST_DIR}/..
${CMAKE_CURRENT_LIST_DIR}/src
)
TARGET_LINK_LIBRARIES(${MODULE_NAME}
  glog::glog 
  ${OpenCV_LIBS} 
  ${Protobuf_LIBRARIES}
  ${INNER_PROTO_TARGET_NAME}
  )

