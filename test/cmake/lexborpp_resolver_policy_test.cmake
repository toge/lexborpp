cmake_minimum_required(VERSION 3.25...3.31)

if(NOT LEXBORPP_SOURCE_DIR)
  message(FATAL_ERROR "LEXBORPP_SOURCE_DIR is required")
endif()
if(NOT LEXBORPP_RESOLVER_POLICY_TEST_DIR)
  message(FATAL_ERROR "LEXBORPP_RESOLVER_POLICY_TEST_DIR is required")
endif()

include("${LEXBORPP_SOURCE_DIR}/cmake/lexborppResolveLexbor.cmake")

function(lexborpp_assert_resolver expected_valid expected_runtime
    candidate is_windows import_suffix static_suffix shared_suffix
    runtime_override)
  lexborpp_validate_raw_shared_library(
    actual_valid actual_runtime actual_error
    "${candidate}" "${is_windows}" "${import_suffix}"
    "${static_suffix}" "${shared_suffix}" "${runtime_override}")

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
  FALSE "" "${ambiguous_library}" TRUE ".lib" ".lib" ".dll" "")

file(WRITE "${runtime_library}" "")
lexborpp_assert_resolver(
  TRUE "${runtime_library}" "${ambiguous_library}" TRUE ".lib" ".lib" ".dll" "")

file(REMOVE "${runtime_library}")
lexborpp_assert_resolver(
  FALSE "" "${ambiguous_library}" TRUE ".lib" ".lib" ".dll" "")
file(WRITE "${runtime_library}" "")
lexborpp_assert_resolver(
  TRUE "${runtime_library}" "${ambiguous_library}"
  TRUE ".lib" ".lib" ".dll" "${runtime_library}")
set(upper_case_runtime
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/bin/lexbor.DLL")
file(WRITE "${upper_case_runtime}" "")
lexborpp_assert_resolver(
  TRUE "${upper_case_runtime}" "${ambiguous_library}"
  TRUE ".lib" ".lib" ".dll" "${upper_case_runtime}")

set(unambiguous_library
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/lib/lexbor.dll.lib")
file(WRITE "${unambiguous_library}" "")
file(REMOVE "${runtime_library}")
lexborpp_assert_resolver(
  TRUE "" "${unambiguous_library}" TRUE ".lib" ".lib" ".dll" "")

set(unambiguous_a_library
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/lib/lexbor.dll.a")
file(WRITE "${unambiguous_a_library}" "")
lexborpp_assert_resolver(
  TRUE "" "${unambiguous_a_library}" TRUE ".dll.a" ".a" ".dll" "")

set(mingw_plain_library
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/lib/liblexbor.a")
set(mingw_runtime_library
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/bin/liblexbor.dll")
file(WRITE "${mingw_plain_library}" "")
lexborpp_assert_resolver(
  FALSE "" "${mingw_plain_library}" TRUE ".dll.a" ".a" ".dll" "")
file(WRITE "${mingw_runtime_library}" "")
lexborpp_assert_resolver(
  TRUE "${mingw_runtime_library}" "${mingw_plain_library}"
  TRUE ".dll.a" ".a" ".dll" "")

set(invalid_runtime
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/bin/lexbor.lib")
file(WRITE "${invalid_runtime}" "")
lexborpp_assert_resolver(
  FALSE "" "${ambiguous_library}" TRUE ".lib" ".lib" ".dll"
  "${invalid_runtime}")
lexborpp_assert_resolver(
  FALSE "" "${ambiguous_library}" TRUE ".lib" ".lib" ".dll"
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/bin")
lexborpp_assert_resolver(
  FALSE "" "${ambiguous_library}" TRUE ".lib" ".lib" ".dll"
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/bin/missing.dll")
file(WRITE "${runtime_library}" "")
lexborpp_assert_resolver(
  TRUE "${runtime_library}" "${ambiguous_library}"
  TRUE ".lib" ".lib" ".dll" "${runtime_library}")

lexborpp_validate_raw_shared_library(
  non_windows_valid non_windows_runtime non_windows_error
  "${ambiguous_library}" FALSE ".a" ".a" ".so" "")
if(NOT non_windows_valid OR non_windows_runtime OR non_windows_error)
  message(FATAL_ERROR
    "non-Windows shared validation changed unexpectedly")
endif()

set(non_windows_runtime
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/lib/liblexbor.so")
file(WRITE "${non_windows_runtime}" "")
lexborpp_validate_raw_shared_library(
  non_windows_override_valid non_windows_override_runtime
  non_windows_override_error "${non_windows_runtime}"
  FALSE ".a" ".a" ".so" "${non_windows_runtime}")
if(NOT non_windows_override_valid OR
    NOT non_windows_override_runtime STREQUAL "${non_windows_runtime}")
  message(FATAL_ERROR
    "non-Windows shared runtime override was rejected: "
    "${non_windows_override_error}")
endif()
foreach(invalid_non_windows_runtime IN ITEMS
    "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/lib/missing.so"
    "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/prefix/lib"
    "${invalid_runtime}")
  lexborpp_validate_raw_shared_library(
    invalid_non_windows_valid invalid_non_windows_runtime
    invalid_non_windows_error "${non_windows_runtime}"
    FALSE ".a" ".a" ".so" "${invalid_non_windows_runtime}")
  if(invalid_non_windows_valid)
    message(FATAL_ERROR
      "non-Windows shared runtime override was accepted: "
      "'${invalid_non_windows_runtime}'")
  endif()
endforeach()

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

set(mingw_prefix "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/mingw-prefix")
set(mingw_include_dir "${mingw_prefix}/include")
set(mingw_library_dir "${mingw_prefix}/lib")
set(mingw_runtime_dir "${mingw_prefix}/bin")
file(MAKE_DIRECTORY
  "${mingw_include_dir}/lexbor/html"
  "${mingw_library_dir}"
  "${mingw_runtime_dir}")
file(WRITE "${mingw_include_dir}/lexbor/html/parser.h" "")
file(WRITE "${mingw_library_dir}/liblexbor.a" "")

set(WIN32 TRUE)
set(CMAKE_IMPORT_LIBRARY_SUFFIX ".dll.a")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".dll")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll.a;.a")
set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
set(CMAKE_INCLUDE_PATH "${mingw_include_dir}")
set(CMAKE_LIBRARY_PATH "${mingw_library_dir}")
set(LEXBORPP_RAW_LEXBOR_RUNTIME_LIBRARY "" CACHE FILEPATH "" FORCE)

lexborpp_resolve_lexbor_dependency(mingw_no_runtime_result)
if(mingw_no_runtime_result_FOUND OR
    NOT mingw_no_runtime_result_NOT_FOUND_MESSAGE MATCHES
      "ambiguous \\.a without matching runtime library")
  message(FATAL_ERROR
    "MinGW raw resolver accepted liblexbor.a without a runtime: "
    "${mingw_no_runtime_result_NOT_FOUND_MESSAGE}")
endif()

file(WRITE "${mingw_runtime_dir}/liblexbor.dll" "")
lexborpp_resolve_lexbor_dependency(mingw_runtime_result)
if(NOT mingw_runtime_result_FOUND OR
    NOT mingw_runtime_result_CONCRETE_LIBRARY STREQUAL
      "${mingw_library_dir}/liblexbor.a" OR
    NOT mingw_runtime_result_RUNTIME_LIBRARY STREQUAL
      "${mingw_runtime_dir}/liblexbor.dll")
  message(FATAL_ERROR
    "MinGW raw resolver did not validate plain liblexbor.a with "
    "liblexbor.dll")
endif()

set(mingw_preferred_prefix
  "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}/mingw-preferred-prefix")
set(mingw_preferred_include_dir "${mingw_preferred_prefix}/include")
set(mingw_preferred_library_dir "${mingw_preferred_prefix}/lib")
file(MAKE_DIRECTORY
  "${mingw_preferred_include_dir}/lexbor/html"
  "${mingw_preferred_library_dir}")
file(WRITE "${mingw_preferred_include_dir}/lexbor/html/parser.h" "")
file(WRITE "${mingw_preferred_library_dir}/liblexbor.a" "")
file(WRITE "${mingw_preferred_library_dir}/liblexbor.dll.lib" "")
file(WRITE "${mingw_preferred_library_dir}/liblexbor.dll.a" "")
set(CMAKE_INCLUDE_PATH "${mingw_preferred_include_dir}")
set(CMAKE_LIBRARY_PATH "${mingw_preferred_library_dir}")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll.a")
find_library(mingw_direct_import NAMES lexbor NO_CACHE)
if(NOT mingw_direct_import STREQUAL
    "${mingw_preferred_library_dir}/liblexbor.dll.a")
  message(FATAL_ERROR
    "direct MinGW import-library lookup failed: '${mingw_direct_import}'")
endif()
lexborpp_resolve_lexbor_dependency(mingw_import_result)
if(NOT mingw_import_result_FOUND OR
    NOT mingw_import_result_CONCRETE_LIBRARY STREQUAL
      "${mingw_preferred_library_dir}/liblexbor.dll.a")
  message(FATAL_ERROR
    "MinGW raw resolver did not prefer liblexbor.dll.a: "
    "found='${mingw_import_result_FOUND}', "
    "library='${mingw_import_result_CONCRETE_LIBRARY}'")
endif()

file(REMOVE_RECURSE "${LEXBORPP_RESOLVER_POLICY_TEST_DIR}")
