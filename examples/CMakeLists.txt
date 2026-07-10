file(GLOB example-sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(source ${example-sources})
  get_filename_component(example "${source}" NAME_WE)
  add_executable(ctjson-example-${example} "${source}")
  target_link_libraries(ctjson-example-${example} PRIVATE ctjson::ctjson)
  add_test(NAME example-${example} COMMAND ctjson-example-${example})
endforeach()
