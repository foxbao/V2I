set(MODULE_NAME zsysd)

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
)

SET(LIBRARIES
utils 
mware 
)

TARGET_LINK_LIBRARIES(${MODULE_NAME} ${LIBRARIES})

