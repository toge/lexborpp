cmake_minimum_required(VERSION 3.25...3.31)
project(lexborpp_consumer LANGUAGES CXX)

find_package(lexborpp CONFIG REQUIRED)
get_property(lexbor_links_before TARGET lexborpp::lexbor
  PROPERTY INTERFACE_LINK_LIBRARIES)
get_property(lexbor_includes_before TARGET lexborpp::lexbor
  PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
get_property(lexbor_defs_before TARGET lexborpp::lexbor
  PROPERTY INTERFACE_COMPILE_DEFINITIONS)

find_package(lexborpp CONFIG REQUIRED)
get_property(lexbor_links_after TARGET lexborpp::lexbor
  PROPERTY INTERFACE_LINK_LIBRARIES)
get_property(lexbor_includes_after TARGET lexborpp::lexbor
  PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
get_property(lexbor_defs_after TARGET lexborpp::lexbor
  PROPERTY INTERFACE_COMPILE_DEFINITIONS)

if(NOT "${lexbor_links_before}" STREQUAL "${lexbor_links_after}" OR
    NOT "${lexbor_includes_before}" STREQUAL "${lexbor_includes_after}" OR
    NOT "${lexbor_defs_before}" STREQUAL "${lexbor_defs_after}")
  message(FATAL_ERROR
    "repeated find_package(lexborpp CONFIG) mutated lexborpp::lexbor "
    "properties: links='${lexbor_links_before}'->'${lexbor_links_after}', "
    "includes='${lexbor_includes_before}'->'${lexbor_includes_after}', "
    "defs='${lexbor_defs_before}'->'${lexbor_defs_after}'")
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE lexborpp::lexborpp)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
  set_target_properties(consumer PROPERTIES CXX_SCAN_FOR_MODULES OFF)
endif()

if(LEXBORPP_EXPECT_NO_MODULE_SCAN AND
    CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
  get_target_property(consumer_scan_for_modules
    consumer CXX_SCAN_FOR_MODULES)
  if(NOT consumer_scan_for_modules STREQUAL "OFF")
    message(FATAL_ERROR
      "consumer must set CXX_SCAN_FOR_MODULES=OFF, got "
      "'${consumer_scan_for_modules}'")
  endif()
endif()

if(LEXBORPP_EXPECT_LINKAGE STREQUAL "STATIC")
  target_compile_definitions(consumer PRIVATE LEXBORPP_EXPECT_STATIC=1)
elseif(LEXBORPP_EXPECT_LINKAGE STREQUAL "SHARED")
  target_compile_definitions(consumer PRIVATE
    LEXBORPP_EXPECT_SHARED=1
    FAKE_LEXBOR_SHARED=1)
endif()

if(LEXBORPP_RUN_QUERY)
  target_compile_definitions(consumer PRIVATE LEXBORPP_RUN_QUERY=1)
endif()

if(LEXBORPP_LINK_PROBE)
  target_compile_definitions(consumer PRIVATE LEXBORPP_LINK_PROBE=1)
  target_include_directories(consumer PRIVATE
    "${LEXBORPP_FAKE_LEXBOR_PROBE_INCLUDE_DIR}")
endif()

if(LEXBORPP_EXPECT_RESOLVED_LIBRARY_SUFFIX)
  if(NOT LEXBORPP_RESOLVED_CONCRETE_LIBRARY MATCHES
      "${LEXBORPP_EXPECT_RESOLVED_LIBRARY_SUFFIX}$")
    message(FATAL_ERROR
      "resolved library '${LEXBORPP_RESOLVED_CONCRETE_LIBRARY}' does not "
      "have expected suffix '${LEXBORPP_EXPECT_RESOLVED_LIBRARY_SUFFIX}'")
  endif()
endif()

if(LEXBORPP_EXPECT_RESOLVED_TARGET)
  set(expected_target_linker_file
    "$<TARGET_LINKER_FILE:${LEXBORPP_EXPECT_RESOLVED_TARGET}>")
  if(NOT LEXBORPP_RESOLVED_CONCRETE_LIBRARY STREQUAL
      expected_target_linker_file)
    message(FATAL_ERROR
      "resolved target metadata '${LEXBORPP_RESOLVED_CONCRETE_LIBRARY}' "
      "does not use TARGET_LINKER_FILE for "
      "'${LEXBORPP_EXPECT_RESOLVED_TARGET}'")
  endif()
endif()

if(LEXBORPP_EXPECT_RESOLVED_RUNTIME_LIBRARY_SUFFIX)
  if(NOT LEXBORPP_RESOLVED_RUNTIME_LIBRARY OR
      NOT LEXBORPP_RESOLVED_RUNTIME_LIBRARY MATCHES
        "${LEXBORPP_EXPECT_RESOLVED_RUNTIME_LIBRARY_SUFFIX}$")
    message(FATAL_ERROR
      "resolved runtime library '${LEXBORPP_RESOLVED_RUNTIME_LIBRARY}' "
      "does not have expected suffix "
      "'${LEXBORPP_EXPECT_RESOLVED_RUNTIME_LIBRARY_SUFFIX}'")
  endif()
endif()

if(LEXBORPP_EXPECT_INTERFACE_ONLY AND
    LEXBORPP_RESOLVED_CONCRETE_LIBRARY)
  message(FATAL_ERROR
    "interface-only config target unexpectedly exposed concrete library "
    "'${LEXBORPP_RESOLVED_CONCRETE_LIBRARY}'")
endif()

enable_testing()
add_test(NAME consumer_runs COMMAND consumer)

if(LEXBORPP_FAKE_LEXBOR_RUNTIME_DIR)
  set_tests_properties(consumer_runs PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:${LEXBORPP_FAKE_LEXBOR_RUNTIME_DIR}")
endif()
