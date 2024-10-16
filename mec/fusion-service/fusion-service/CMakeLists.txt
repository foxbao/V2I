
FILE( GLOB CPPS [^.]*.cpp [^.]*.cc)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)

target_sources(fusion_service
PRIVATE
${HPPS}
${CPPS}
)