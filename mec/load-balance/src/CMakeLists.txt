
FILE( GLOB CPPS [^.]*.cpp [^.]*.cc)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)

target_sources(${PROJECT_NAME}
PRIVATE
${HPPS}
${CPPS}
)