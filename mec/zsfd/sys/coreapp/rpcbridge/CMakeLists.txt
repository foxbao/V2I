set(MODULE_NAME rpcbridge)

link_directories(
    ${EXTERNAL_LIB_ROOT_PATH}/lib
)

add_executable(${MODULE_NAME})

FILE( GLOB CPPS [^.]*.cpp [^.]*.cc)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)

target_sources(${MODULE_NAME}
PRIVATE
${HPPS}
${CPPS}
)
# set the include path
target_include_directories(${MODULE_NAME}
PUBLIC
${CMAKE_CURRENT_LIST_DIR}/inc
${CMAKE_CURRENT_LIST_DIR}/../../../inc
${EXTERNAL_LIB_ROOT_PATH}/include
)

SET(LIBRARIES
utils 
mware 
libpaho-mqtt3a.so
)

TARGET_LINK_LIBRARIES(${MODULE_NAME} ${LIBRARIES})

