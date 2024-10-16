file(GLOB CPPS [^.]*.cpp [^.]*.cc)
file(GLOB HPPS [^.]*.hpp [^.]*.h)

target_sources(${MODULE_NAME} PRIVATE ${HPPS} ${CPPS})
