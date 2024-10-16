set(MODULE_NAME mware)

add_library(${MODULE_NAME} SHARED)

FILE( GLOB CPPS [^.]*.cpp [^.]*.cc)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)


find_package(Protobuf REQUIRED)
SET(PROTO_FILE_ROOT_PATH ${CMAKE_CURRENT_LIST_DIR}/rpc)
SET(PROTO_TARGET_NAME ${MODULE_NAME}_proto_target)
SET(PROTO_FILE_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
include(../cmake/options.cmake)
include(../cmake/functions.cmake)
include(../cmake/process_proto.cmake)

aux_source_directory(rpc MAWARE_RPC_LIST)

target_sources(${MODULE_NAME}
PRIVATE
${HPPS}
${CPPS}
${MAWARE_RPC_LIST}
)

# set the include path
target_include_directories(${MODULE_NAME}
PUBLIC
${CMAKE_CURRENT_LIST_DIR}/inc
${CMAKE_CURRENT_LIST_DIR}/../inc/mware
${CMAKE_CURRENT_LIST_DIR}/../inc
${Protobuf_INCLUDE_DIRS}
${CMAKE_CURRENT_BINARY_DIR}
)

SET(LIBRARIES
utils
libudev.so
)


TARGET_LINK_LIBRARIES(${MODULE_NAME} ${LIBRARIES} ${Protobuf_LIBRARIES} ${PROTO_TARGET_NAME})

