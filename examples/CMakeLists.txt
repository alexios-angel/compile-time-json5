file(GLOB example-sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(source ${example-sources})
  get_filename_component(example "${source}" NAME_WE)
  add_executable(ctjson5-example-${example} "${source}")
  if(example STREQUAL "cxx17-syntax")
    # demonstrates the C++17 API, so it must not inherit the interface
    # target's cxx_std requirement
    target_include_directories(ctjson5-example-${example} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    set_target_properties(ctjson5-example-${example} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  else()
    target_link_libraries(ctjson5-example-${example} PRIVATE ctjson5::ctjson5)
  endif()
  add_test(NAME example-${example} COMMAND ctjson5-example-${example})
endforeach()
