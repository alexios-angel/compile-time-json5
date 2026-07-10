# the suite is static_asserts: compiling a test IS running it
add_custom_target(ctjson5-test ALL)

file(GLOB test-sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# tests with a main() run; the rest are static_assert suites where
# compiling is the test
set(runtime-tests dumps load)

foreach(source ${test-sources})
  get_filename_component(test "${source}" NAME_WE)
  if(test IN_LIST runtime-tests)
    add_executable(ctjson5-test-${test} "${source}")
    target_link_libraries(ctjson5-test-${test} PRIVATE ctjson5::ctjson5)
    add_dependencies(ctjson5-test ctjson5-test-${test})
    add_test(NAME run-${test} COMMAND ctjson5-test-${test})
  else()
    add_library(ctjson5-test-${test} OBJECT EXCLUDE_FROM_ALL "${source}")
    target_link_libraries(ctjson5-test-${test} PRIVATE ctjson5::ctjson5)
    add_dependencies(ctjson5-test ctjson5-test-${test})
    add_test(NAME compile-${test}
      COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target ctjson5-test-${test})
  endif()
endforeach()
