file(GLOB example-sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(source ${example-sources})
  get_filename_component(example "${source}" NAME_WE)
  add_executable(ctjson5-example-${example} "${source}")
  if(example STREQUAL "cxx17-syntax")
    # demonstrates the C++17 API, so it must not inherit the interface
    # target's cxx_std requirement
    target_include_directories(ctjson5-example-${example} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    set_target_properties(ctjson5-example-${example} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    if(CTJSON5_CONSTEXPR_LIMITS)
      target_compile_options(ctjson5-example-${example} PRIVATE ${CTJSON5_CONSTEXPR_OPTIONS})
    endif()
  else()
    target_link_libraries(ctjson5-example-${example} PRIVATE ctjson5::ctjson5)
    if(CTJSON5_PCH AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
      target_precompile_headers(ctjson5-example-${example} PRIVATE
        "${PROJECT_SOURCE_DIR}/include/ctjson5.hpp")
    endif()
  endif()
  add_test(NAME example-${example} COMMAND ctjson5-example-${example})
endforeach()
