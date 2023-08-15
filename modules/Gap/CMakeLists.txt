# set(MODULE_NAME ${PROJECT_NAME}_gap)

# add_library(${MODULE_NAME} SHARED)
FILE( GLOB CPPS [^.]*.cpp [^.]*.cc)
FILE( GLOB HPPS [^.]*.hpp [^.]*.h)

target_sources(${MODULE_NAME}
PRIVATE
    ${HPPS}
    ${CPPS}
)

# target_include_directories(${MODULE_NAME}
# PUBLIC
#     ${CMAKE_CURRENT_LIST_DIR}/
# )

# set(target_name ${MODULE_NAME}_test)
# add_executable(${target_name} gap_test.cpp)
# target_link_libraries(
# 	${target_name}
# 	PRIVATE
# 	${MODULE_NAME}
# 	)

# add_executable(mle_test mle_test.cpp)
# # target_link_libraries(
# # 	${target_name}
# # 	PRIVATE
# # 	${MODULE_NAME}
# # 	)
