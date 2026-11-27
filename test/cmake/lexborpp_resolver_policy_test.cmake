cmake_minimum_required(VERSION 3.25...3.31)

if(NOT LEXBORPP_SOURCE_DIR)
  message(FATAL_ERROR "LEXBORPP_SOURCE_DIR is required")
endif()
if(NOT LEXBORPP_RESOLVER_POLICY_TEST_DIR)
  message(FATAL_ERROR "LEXBORPP_RESOLVER_POLICY_TEST_DIR is required")
endif()

include("${LEXBORPP_SOURCE_DIR}/cmake/lexborppResolveLexbor.cmake")

function(lexborpp_assert_resolver expected_valid expected_runtime
    candidate runtime_override)
  lexborpp_validate_raw_shared_library(
    actual_valid actual_runtime actual_error
    "${candidate}" TRUE ".lib" ".dll" "${runtime_override}")

  if(NOT actual_valid STREQUAL expected_valid)
    message(FATAL_ERROR
      "unexpected validation result for '${candidate}': expected "
      "${expected_valid}, got ${actual_valid}: ${actual_error}")
  endif()
  if(NOT actual_runtime STREQUAL expected_runtime)
    message(FATAL_ERROR
      "unexpected runtime for '${candidate}': expected "
      "'${expected_runtime}', got '${actual_runtime}'")
  endif()
endfunction()

file(REMOVE_RECURSE "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}")
file(MAKE_DIRECTORY
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/lib"
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/bin")

set(ambiguous_library
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/lib/lexbor.lib")
set(runtime_library
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/bin/lexbor.dll")
file(WRITE "${ambiguous_library}" "")

lexborpp_assert_resolver(
  FALSE "" "${ambiguous_library}" "")

file(WRITE "${runtime_library}" "")
lexborpp_assert_resolver(
  TRUE "${runtime_library}" "${ambiguous_library}" "")

file(REMOVE "${runtime_library}")
lexborpp_assert_resolver(
  FALSE "" "${ambiguous_library}" "")
file(WRITE "${runtime_library}" "")
lexborpp_assert_resolver(
  TRUE "${runtime_library}" "${ambiguous_library}" "${runtime_library}")

set(unambiguous_library
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/lib/lexbor.dll.lib")
file(WRITE "${unambiguous_library}" "")
file(REMOVE "${runtime_library}")
lexborpp_assert_resolver(
  TRUE "" "${unambiguous_library}" "")

set(unambiguous_a_library
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/lib/lexbor.dll.a")
file(WRITE "${unambiguous_a_library}" "")
lexborpp_assert_resolver(
  TRUE "" "${unambiguous_a_library}" "")

lexborpp_validate_raw_shared_library(
  non_windows_valid non_windows_runtime non_windows_error
  "${ambiguous_library}" FALSE ".a" ".so" "")
if(NOT non_windows_valid OR non_windows_runtime OR non_windows_error)
  message(FATAL_ERROR
    "non-Windows shared validation changed unexpectedly")
endif()

set(raw_prefix "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/raw-prefix")
set(raw_include_dir "${raw_prefix}/include")
set(raw_library_dir "${raw_prefix}/lib")
set(raw_runtime_dir "${raw_prefix}/bin")
file(MAKE_DIRECTORY
  "${raw_include_dir}/lexbor/html"
  "${raw_library_dir}"
  "${raw_runtime_dir}")
file(WRITE "${raw_include_dir}/lexbor/html/parser.h" "")
file(WRITE "${raw_library_dir}/lexbor.lib" "")

set(WIN32 TRUE)
set(CMAKE_IMPORT_LIBRARY_SUFFIX ".lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".lib")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".dll")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
set(CMAKE_FIND_LIBRARY_PREFIXES "")
set(CMAKE_DISABLE_FIND_PACKAGE_lexbor TRUE)
set(CMAKE_INCLUDE_PATH "${raw_include_dir}")
set(CMAKE_LIBRARY_PATH "${raw_library_dir}")
set(LEXBORPP_RAW_LEXBOR_LINKAGE "SHARED" CACHE STRING "" FORCE)

lexborpp_resolve_lexbor_dependency(no_runtime_result)
if(no_runtime_result_FOUND OR
    NOT no_runtime_result_NOT_FOUND_MESSAGE MATCHES
      "ambiguous.*without matching runtime library")
  message(FATAL_ERROR
    "raw resolver accepted an ambiguous static-only .lib: "
    "${no_runtime_result_NOT_FOUND_MESSAGE}")
endif()

file(WRITE "${raw_runtime_dir}/lexbor.dll" "")
lexborpp_resolve_lexbor_dependency(runtime_result)
if(NOT runtime_result_FOUND OR
    NOT runtime_result_LINKAGE STREQUAL "SHARED" OR
    NOT runtime_result_RUNTIME_LIBRARY STREQUAL
      "${raw_runtime_dir}/lexbor.dll")
  message(FATAL_ERROR
    "raw resolver did not accept an ambiguous .lib with a matching DLL")
endif()

file(REMOVE "${raw_runtime_dir}/lexbor.dll")
set(explicit_runtime
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/explicit-runtime/lexbor.dll")
file(MAKE_DIRECTORY
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/explicit-runtime")
file(WRITE "${explicit_runtime}" "")
set(LEXBORPP_RAW_LEXBOR_RUNTIME_LIBRARY "${explicit_runtime}"
  CACHE FILEPATH "" FORCE)
lexborpp_resolve_lexbor_dependency(explicit_runtime_result)
if(NOT explicit_runtime_result_FOUND OR
    NOT explicit_runtime_result_RUNTIME_LIBRARY STREQUAL
      "${explicit_runtime}")
  message(FATAL_ERROR
    "raw resolver did not honor an explicit runtime library")
endif()

file(REMOVE_RECURSE "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}")
