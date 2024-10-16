function(generate_proto_files ret_proto_files) 
  # ---- 设置提取变量 ----
  set(prefix proto_interface)
  set(noValues)
  set(singleValues PROTO_OUTPUT_DIR PROTO_INPUT_DIR)
  set(multiValues PROTO_PATHS PROTO_FILES)

  # ---- 解析变量 ----
  include(CMakeParseArguments )
  cmake_parse_arguments(${prefix}
    "${noValues}"
    "${singleValues}"
    "${multiValues}"
    ${ARGN}
  )

  # ---- 一些变量前置检查 ----
  if(NOT proto_interface_PROTO_OUTPUT_DIR)
    message(FATAL_ERROR "Not define proto output dir !!!")
  else()
    message(STATUS "proto output dir is: ${proto_interface_PROTO_OUTPUT_DIR}")
  endif()

  if ((NOT proto_interface_PROTO_INPUT_DIR) AND (NOT EXISTS ${proto_interface_PROTO_INPUT_DIR}))
    message(FATAL_ERROR "Not define proto input dir !!!")
  else()
    message(STATUS "proto input dir is: ${proto_interface_PROTO_INPUT_DIR}")
  endif()

  if(NOT proto_interface_PROTO_PATHS)
    message(FATAL_ERROR "Not define proto paths !!!")
  else()
    message(STATUS "proto paths: ${proto_interface_PROTO_PATHS}")
  endif() 

  if(NOT proto_interface_PROTO_FILES)
    message(FATAL_ERROR "Not define proto files !!!")
  endif() 

  if (NOT EXISTS ${proto_interface_PROTO_OUTPUT_DIR})
    message(STATUS "Create proto output dir ${proto_interface_PROTO_OUTPUT_DIR}")
    file(MAKE_DIRECTORY ${proto_interface_PROTO_OUTPUT_DIR})
  endif()

  #construct cmd
  set(final_exec_cmd ${PROTOBUF_PROTOC_EXECUTABLE})
  foreach(proto_path_opt ${proto_interface_PROTO_PATHS})
    set(final_exec_cmd "${final_exec_cmd};--proto_path=${proto_path_opt}")
    set(final_python_exec_cmd "${final_exec_cmd}")
  endforeach()
  set(final_exec_cmd "${final_exec_cmd};--cpp_out=${proto_interface_PROTO_OUTPUT_DIR}")
  set(final_python_exec_cmd "${final_python_exec_cmd};--python_out=${CMAKE_SOURCE_DIR}/output/python")

  foreach(proto_file_item ${proto_interface_PROTO_FILES})
    #检查输入文件是否存在
    if(NOT EXISTS ${proto_interface_PROTO_INPUT_DIR}/${proto_file_item})
      message(FATAL_ERROR "proto file not exist:  ${proto_interface_PROTO_INPUT_DIR}/${proto_file_item}")
    endif()

    #检查目标文件是否已经生成
    file(TO_NATIVE_PATH  ${proto_interface_PROTO_INPUT_DIR}/${proto_file_item} proto_native_path)
    string(REPLACE ".proto" ".pb.cc" proto_dest_file ${proto_native_path})
    #string(REPLACE "${proto_interface_PROTO_INPUT_DIR}" "${proto_interface_PROTO_OUTPUT_DIR}" proto_dest_file ${proto_dest_file})
    string(REGEX REPLACE "^${proto_interface_PROTO_INPUT_DIR}" "${proto_interface_PROTO_OUTPUT_DIR}" proto_dest_file ${proto_dest_file})
    if((NOT EXISTS ${proto_dest_file}) OR (${proto_native_path} IS_NEWER_THAN ${proto_dest_file}))
      execute_process(COMMAND ${final_exec_cmd} ${proto_native_path})
      #message(STATUS ${final_exec_cmd} == ${proto_native_path})
      #execute_process(COMMAND ${final_python_exec_cmd} ${proto_native_path}) #liyuan 20210902 generate py file for python tools to use
    endif()
    list(APPEND proto_source_file_list ${proto_dest_file})
  endforeach()
  #返回pb.cc的文件列表
  set(${ret_proto_files} ${proto_source_file_list} PARENT_SCOPE)
endfunction()