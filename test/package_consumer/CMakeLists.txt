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
    "${LEXBORPP_FAKE_LEXBOR_INCLUDE_DIR}")
endif()

enable_testing()
add_test(NAME consumer_runs COMMAND consumer)

if(LEXBORPP_FAKE_LEXBOR_RUNTIME_DIR)
  set_tests_properties(consumer_runs PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:${LEXBORPP_FAKE_LEXBOR_RUNTIME_DIR}")
endif()
