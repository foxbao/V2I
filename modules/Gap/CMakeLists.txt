# set(MODULE_NAME ${PROJECT_NAME}_gap)

# add_library(${MODULE_NAME} SHARED)
file(GLOB CPPS [^.]*.cpp [^.]*.cc)
file(GLOB HPPS [^.]*.hpp [^.]*.h)

target_sources(${MODULE_NAME} PRIVATE ${HPPS} ${CPPS})

# set(target_name GAP_test) add_executable(${target_name} gap_test.cpp)
# target_link_libraries( ${target_name} PRIVATE ${MODULE_NAME} )

# add_executable(mle_test mle_test.cpp) # target_link_libraries( #
# ${target_name} #       PRIVATE #       ${MODULE_NAME} #       )
