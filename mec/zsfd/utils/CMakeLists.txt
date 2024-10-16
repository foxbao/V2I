set(MODULE_NAME utils)

link_directories(
    ${EXTERNAL_LIB_ROOT_PATH}/lib
)
add_library(${MODULE_NAME} SHARED)
add_definitions("-DLIBUTILS")

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
${CMAKE_CURRENT_LIST_DIR}/../inc
${EXTERNAL_LIB_ROOT_PATH}/include
)

SET(LIBRARIES
librt.so
libuuid.so
libcurl.so
libssl.so
libcrypto.so
libsqlite3.so
)
TARGET_LINK_LIBRARIES(${MODULE_NAME} ${LIBRARIES})

