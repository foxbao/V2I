file(GLOB CPPS [^.]*.cpp [^.]*.cc)
file(GLOB HPPS [^.]*.hpp [^.]*.h)

target_sources(${MODULE_NAME} PRIVATE ${HPPS} ${CPPS})

# set(target_name HMM_test) add_executable(${target_name} hmm_test.cpp)
# target_link_libraries( ${target_name} PRIVATE ${MODULE_NAME} )
