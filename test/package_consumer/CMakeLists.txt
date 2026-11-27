cmake_minimum_required(VERSION 3.25...3.31)
project(lexborpp_consumer LANGUAGES CXX)

find_package(lexborpp CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE lexborpp::lexborpp)

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
