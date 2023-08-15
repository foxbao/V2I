set(MODULE_NAME ${PROJECT_NAME}_view)

find_package(OpenCV REQUIRED)
add_library(${MODULE_NAME} SHARED)

FILE( GLOB CPPS [^.]*.cpp [^.]*.cc)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)

target_sources(${MODULE_NAME}
PRIVATE
    ${HPPS}
    ${CPPS}
)


TARGET_LINK_LIBRARIES(${MODULE_NAME} 
PUBLIC
    ${OpenCV_LIBS} 
PRIVATE
    
)

