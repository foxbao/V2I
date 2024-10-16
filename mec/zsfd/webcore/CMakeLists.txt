set(MODULE_NAME webcore)

add_library(${MODULE_NAME} SHARED)

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
${CMAKE_CURRENT_LIST_DIR}/../inc/webcore
)

SET(LIBRARIES
libzmq.so
utils
)
TARGET_LINK_LIBRARIES(${MODULE_NAME} ${LIBRARIES})

