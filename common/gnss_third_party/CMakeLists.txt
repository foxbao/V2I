FILE( GLOB CPPS [^.]*.cpp [^.]*.cc [^.]*.c)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)

target_sources(${MODULE_NAME}
PUBLIC
${HPPS}
${CPPS}
)
