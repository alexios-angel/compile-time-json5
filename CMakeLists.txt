cmake_minimum_required(VERSION 3.14)

project(ctjson
  HOMEPAGE_URL "https://github.com/alexios-angel/compile-time-json"
  VERSION 0.1.0
  LANGUAGES CXX)
set(PROJECT_DESCRIPTION "Compile-time JSON: parse while you compile, query with constexpr accessors.")

include(GNUInstallDirs)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

if (NOT CTJSON_CXX_STANDARD)
  set(CTJSON_CXX_STANDARD 20)
endif()

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_${CTJSON_CXX_STANDARD})

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}-targets)
install(DIRECTORY include/ DESTINATION include FILES_MATCHING PATTERN *.hpp)
