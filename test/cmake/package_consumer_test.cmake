cmake_minimum_required(VERSION 3.25...3.31)

set(required_variables
  LEXBORPP_SOURCE_DIR
  LEXBORPP_BINARY_DIR
  LEXBORPP_TEST_CONFIG
  LEXBORPP_REAL_LEXBOR_LIBRARY
  LEXBORPP_LEXBOR_INCLUDE_DIR
  LEXBORPP_GENERATOR
  LEXBORPP_C_COMPILER
  LEXBORPP_CXX_COMPILER
  LEXBORPP_CTEST_COMMAND)
foreach(variable IN LISTS required_variables)
  if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
    message(FATAL_ERROR "package_consumer_matrix requires ${variable}")
  endif()
endforeach()

set(work_dir "${LEXBORPP_BINARY_DIR}/package_consumer_matrix")
file(REMOVE_RECURSE "${work_dir}")
file(MAKE_DIRECTORY "${work_dir}")

function(lexborpp_generator_args result)
  set(args -G "${LEXBORPP_GENERATOR}")
  if(LEXBORPP_GENERATOR_INSTANCE)
    list(APPEND args
      "-DCMAKE_GENERATOR_INSTANCE=${LEXBORPP_GENERATOR_INSTANCE}")
  endif()
  if(LEXBORPP_GENERATOR_PLATFORM)
    list(APPEND args -A "${LEXBORPP_GENERATOR_PLATFORM}")
  endif()
  if(LEXBORPP_GENERATOR_TOOLSET)
    list(APPEND args -T "${LEXBORPP_GENERATOR_TOOLSET}")
  endif()
  if(LEXBORPP_C_COMPILER)
    list(APPEND args "-DCMAKE_C_COMPILER=${LEXBORPP_C_COMPILER}")
  endif()
  if(LEXBORPP_CXX_COMPILER)
    list(APPEND args "-DCMAKE_CXX_COMPILER=${LEXBORPP_CXX_COMPILER}")
  endif()
  if(LEXBORPP_MAKE_PROGRAM)
    list(APPEND args "-DCMAKE_MAKE_PROGRAM=${LEXBORPP_MAKE_PROGRAM}")
  endif()
  if(LEXBORPP_MSVC_RUNTIME_LIBRARY)
    list(APPEND args
      "-DCMAKE_MSVC_RUNTIME_LIBRARY=${LEXBORPP_MSVC_RUNTIME_LIBRARY}")
  endif()
  if(LEXBORPP_OSX_ARCHITECTURES)
    list(APPEND args
      "-DCMAKE_OSX_ARCHITECTURES=${LEXBORPP_OSX_ARCHITECTURES}")
  endif()
  list(APPEND args
    "-DCMAKE_BUILD_TYPE=${LEXBORPP_TEST_CONFIG}")
  set(${result} "${args}" PARENT_SCOPE)
endfunction()

lexborpp_generator_args(generator_args)

function(lexborpp_run_configure source binary result output)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${binary}"
      ${generator_args} ${ARGN}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error)
  set(${result} "${configure_result}" PARENT_SCOPE)
  set(${output}
      "${configure_output}\n${configure_error}" PARENT_SCOPE)
endfunction()

function(lexborpp_require_configure source binary label)
  lexborpp_run_configure("${source}" "${binary}" result output ${ARGN})
  if(NOT result EQUAL 0)
    message(FATAL_ERROR
      "${label} configure failed (${result}):\n${output}")
  endif()
endfunction()

function(lexborpp_require_configure_failure source binary label match)
  lexborpp_run_configure("${source}" "${binary}" result output ${ARGN})
  if(result EQUAL 0)
    message(FATAL_ERROR "${label} unexpectedly configured successfully")
  endif()
  if(NOT output MATCHES "${match}")
    message(FATAL_ERROR
      "${label} failed without the expected diagnostic '${match}':\n${output}")
  endif()
endfunction()

function(lexborpp_run_build binary label)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${binary}"
      --config "${LEXBORPP_TEST_CONFIG}" --parallel
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error)
  if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
      "${label} build failed (${build_result}):\n"
      "${build_output}\n${build_error}")
  endif()
endfunction()

function(lexborpp_run_install binary prefix label)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${binary}"
      --prefix "${prefix}" --config "${LEXBORPP_TEST_CONFIG}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)
  if(NOT install_result EQUAL 0)
    message(FATAL_ERROR
      "${label} install failed (${install_result}):\n"
      "${install_output}\n${install_error}")
  endif()
endfunction()

function(lexborpp_run_ctest binary label)
  execute_process(
    COMMAND "${LEXBORPP_CTEST_COMMAND}" --test-dir "${binary}"
      -C "${LEXBORPP_TEST_CONFIG}" --output-on-failure
    RESULT_VARIABLE test_result
    OUTPUT_VARIABLE test_output
    ERROR_VARIABLE test_error)
  if(NOT test_result EQUAL 0)
    message(FATAL_ERROR
      "${label} CTest failed (${test_result}):\n"
      "${test_output}\n${test_error}")
  endif()
endfunction()

function(lexborpp_configure_build_test source binary label)
  lexborpp_require_configure("${source}" "${binary}" "${label}" ${ARGN})
  lexborpp_run_build("${binary}" "${label}")
  lexborpp_run_ctest("${binary}" "${label}")
endfunction()

set(install_prefix "${work_dir}/install")
lexborpp_run_install(
  "${LEXBORPP_BINARY_DIR}" "${install_prefix}" "lexborpp")

file(GLOB_RECURSE installed_targets
  "${install_prefix}/lexborppTargets.cmake")
list(LENGTH installed_targets installed_targets_count)
if(NOT installed_targets_count EQUAL 1)
  message(FATAL_ERROR
    "expected one installed lexborppTargets.cmake, found "
    "${installed_targets_count}: ${installed_targets}")
endif()
list(GET installed_targets 0 lexborpp_targets_file)
file(READ "${lexborpp_targets_file}" lexborpp_targets)

if(lexborpp_targets MATCHES
    "(^|[^A-Za-z0-9_])lexbor::lexbor_static([^A-Za-z0-9_]|$)" OR
    lexborpp_targets MATCHES
    "(^|[^A-Za-z0-9_])lexbor::lexbor([^A-Za-z0-9_]|$)")
  message(FATAL_ERROR
    "installed ${lexborpp_targets_file} leaks an external Lexbor target")
endif()
foreach(leaked_path
    "${LEXBORPP_SOURCE_DIR}"
    "${LEXBORPP_BINARY_DIR}"
    "${LEXBORPP_LEXBOR_INCLUDE_DIR}"
    "${LEXBORPP_REAL_LEXBOR_LIBRARY}")
  string(FIND "${lexborpp_targets}" "${leaked_path}" leaked_path_index)
  if(NOT leaked_path STREQUAL "" AND NOT leaked_path_index EQUAL -1)
    message(FATAL_ERROR
      "installed ${lexborpp_targets_file} leaks absolute path: ${leaked_path}")
  endif()
endforeach()
if(NOT lexborpp_targets MATCHES "lexborpp::lexbor")
  message(FATAL_ERROR
    "installed ${lexborpp_targets_file} does not export lexborpp::lexbor")
endif()

get_filename_component(lexborpp_config_dir
  "${lexborpp_targets_file}" DIRECTORY)
set(consumer_source "${LEXBORPP_SOURCE_DIR}/test/package_consumer")
set(fake_config_dir
  "${LEXBORPP_SOURCE_DIR}/test/cmake/fake_lexbor_config")
set(raw_source "${LEXBORPP_SOURCE_DIR}/test/cmake/fake_lexbor_raw")
set(empty_prefix "${work_dir}/empty-prefix")
file(MAKE_DIRECTORY "${empty_prefix}/include" "${empty_prefix}/lib")

set(raw_isolation_args
  "-DCMAKE_DISABLE_FIND_PACKAGE_lexbor=TRUE"
  "-DCMAKE_PREFIX_PATH=${empty_prefix}"
  "-DCMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH=FALSE"
  "-DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=FALSE"
  "-DCMAKE_FIND_USE_INSTALL_PREFIX=FALSE"
  "-DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH=FALSE"
  "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE"
  "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE")

set(config_toolchain_args)
if(LEXBORPP_TOOLCHAIN_FILE)
  list(APPEND config_toolchain_args
    "-DCMAKE_TOOLCHAIN_FILE=${LEXBORPP_TOOLCHAIN_FILE}")
endif()
if(LEXBORPP_VCPKG_TARGET_TRIPLET)
  list(APPEND config_toolchain_args
    "-DVCPKG_TARGET_TRIPLET=${LEXBORPP_VCPKG_TARGET_TRIPLET}")
endif()
if(LEXBORPP_VCPKG_INSTALLED_DIR)
  list(APPEND config_toolchain_args
    "-DVCPKG_INSTALLED_DIR=${LEXBORPP_VCPKG_INSTALLED_DIR}")
endif()

lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-vcpkg"
  "installed vcpkg consumer"
  "-Dlexborpp_DIR=${lexborpp_config_dir}"
  "-DLEXBORPP_EXPECT_LINKAGE=STATIC"
  "-DLEXBORPP_RUN_QUERY=ON"
  ${config_toolchain_args})

lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-fake-config"
  "installed fake-config consumer"
  "-Dlexborpp_DIR=${lexborpp_config_dir}"
  "-Dlexbor_DIR=${fake_config_dir}"
  "-DLEXBORPP_FAKE_LEXBOR_INCLUDE_DIR=${LEXBORPP_LEXBOR_INCLUDE_DIR}"
  "-DLEXBORPP_FAKE_LEXBOR_LIBRARY=${LEXBORPP_REAL_LEXBOR_LIBRARY}"
  "-DLEXBORPP_FAKE_LEXBOR_COMPILE_DEFINITIONS=LEXBOR_STATIC"
  "-DLEXBORPP_EXPECT_LINKAGE=STATIC"
  "-DLEXBORPP_RUN_QUERY=ON")

set(fake_static_explicit_prefix "${work_dir}/fake-static-explicit")
set(fake_static_generic_prefix "${work_dir}/fake-static-generic")
set(fake_shared_generic_prefix "${work_dir}/fake-shared-generic")

function(lexborpp_build_fake kind prefix label)
  set(binary "${work_dir}/build-${label}")
  lexborpp_require_configure(
    "${raw_source}" "${binary}" "${label}"
    "-DFAKE_LEXBOR_KIND=${kind}"
    "-DCMAKE_INSTALL_PREFIX=${prefix}")
  lexborpp_run_build("${binary}" "${label}")
  lexborpp_run_install("${binary}" "${prefix}" "${label}")
  file(COPY "${LEXBORPP_LEXBOR_INCLUDE_DIR}/lexbor"
    DESTINATION "${prefix}/include")
endfunction()

lexborpp_build_fake(
  STATIC_EXPLICIT "${fake_static_explicit_prefix}" "fake-static-explicit")
lexborpp_build_fake(
  STATIC_GENERIC "${fake_static_generic_prefix}" "fake-static-generic")
lexborpp_build_fake(
  SHARED_GENERIC "${fake_shared_generic_prefix}" "fake-shared-generic")

if(LEXBORPP_SYSTEM_NAME STREQUAL "Windows")
  set(raw_shared_runtime_dir "${fake_shared_generic_prefix}/bin")
  if(LEXBORPP_IMPORT_LIBRARY_SUFFIX)
    set(raw_shared_suffix "${LEXBORPP_IMPORT_LIBRARY_SUFFIX}")
  else()
    set(raw_shared_suffix "${LEXBORPP_STATIC_LIBRARY_SUFFIX}")
  endif()
else()
  set(raw_shared_runtime_dir "${fake_shared_generic_prefix}/lib")
  set(raw_shared_suffix "${LEXBORPP_SHARED_LIBRARY_SUFFIX}")
endif()

function(lexborpp_raw_consumer_args prefix suffix result)
  set(args
    ${raw_isolation_args}
    "-Dlexborpp_DIR=${lexborpp_config_dir}"
    "-DCMAKE_INCLUDE_PATH=${prefix}/include"
    "-DCMAKE_LIBRARY_PATH=${prefix}/lib"
    "-DCMAKE_FIND_LIBRARY_SUFFIXES=${suffix}"
    "-DLEXBORPP_LINK_PROBE=ON"
    "-DLEXBORPP_FAKE_LEXBOR_INCLUDE_DIR=${prefix}/include")
  set(${result} "${args}" PARENT_SCOPE)
endfunction()

lexborpp_raw_consumer_args(
  "${fake_static_explicit_prefix}"
  "${LEXBORPP_STATIC_LIBRARY_SUFFIX}"
  raw_explicit_args)
lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-raw-explicit-auto"
  "raw explicit static AUTO consumer"
  ${raw_explicit_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=AUTO"
  "-DLEXBORPP_EXPECT_LINKAGE=STATIC")

lexborpp_raw_consumer_args(
  "${fake_static_generic_prefix}"
  "${LEXBORPP_STATIC_LIBRARY_SUFFIX}"
  raw_static_args)
lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-raw-generic-static"
  "raw generic STATIC consumer"
  ${raw_static_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=STATIC"
  "-DLEXBORPP_EXPECT_LINKAGE=STATIC")

lexborpp_raw_consumer_args(
  "${fake_shared_generic_prefix}"
  "${raw_shared_suffix}"
  raw_shared_args)
lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-raw-generic-shared"
  "raw generic SHARED consumer"
  ${raw_shared_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=SHARED"
  "-DLEXBORPP_EXPECT_LINKAGE=SHARED"
  "-DLEXBORPP_FAKE_LEXBOR_RUNTIME_DIR=${raw_shared_runtime_dir}")

lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-raw-generic-auto-shared"
  "raw generic AUTO-as-shared consumer"
  ${raw_shared_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=AUTO"
  "-DLEXBORPP_EXPECT_LINKAGE=SHARED"
  "-DLEXBORPP_FAKE_LEXBOR_RUNTIME_DIR=${raw_shared_runtime_dir}")

lexborpp_require_configure_failure(
  "${consumer_source}" "${work_dir}/consumer-invalid-linkage"
  "invalid raw linkage" "LEXBORPP_RAW_LEXBOR_LINKAGE"
  ${raw_static_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=dynamic")

lexborpp_require_configure_failure(
  "${consumer_source}" "${work_dir}/consumer-shared-only-static"
  "SHARED with only explicit static artifact"
  "lexbor.*shared|library.*lexbor"
  ${raw_explicit_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=SHARED")

lexborpp_require_configure_failure(
  "${consumer_source}" "${work_dir}/consumer-missing"
  "missing Lexbor dependency"
  "lexborpp.*[Nn]ot [Ff]ound|Lexbor.*[Nn]ot [Ff]ound|could not.*Lexbor"
  "-Dlexborpp_DIR=${lexborpp_config_dir}"
  ${raw_isolation_args}
  "-DCMAKE_INCLUDE_PATH=${empty_prefix}/include"
  "-DCMAKE_LIBRARY_PATH=${empty_prefix}/lib")

set(add_subdirectory_source "${work_dir}/add-subdirectory-consumer")
file(MAKE_DIRECTORY "${add_subdirectory_source}")
file(WRITE "${add_subdirectory_source}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.25...3.31)
project(lexborpp_add_subdirectory_consumer LANGUAGES CXX)

set(BUILD_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_BENCH OFF CACHE BOOL "" FORCE)
add_subdirectory("@LEXBORPP_SOURCE_DIR@" third_party/lexborpp)

add_executable(consumer "@LEXBORPP_SOURCE_DIR@/test/package_consumer/main.cpp")
target_link_libraries(consumer PRIVATE lexborpp::lexborpp)
if(LEXBORPP_EXPECT_LINKAGE STREQUAL "STATIC")
  target_compile_definitions(consumer PRIVATE LEXBORPP_EXPECT_STATIC=1)
elseif(LEXBORPP_EXPECT_LINKAGE STREQUAL "SHARED")
  target_compile_definitions(consumer PRIVATE LEXBORPP_EXPECT_SHARED=1)
endif()
if(LEXBORPP_RUN_QUERY)
  target_compile_definitions(consumer PRIVATE LEXBORPP_RUN_QUERY=1)
endif()
if(LEXBORPP_LINK_PROBE)
  target_compile_definitions(consumer PRIVATE
    LEXBORPP_LINK_PROBE=1)
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
]=])
configure_file(
  "${add_subdirectory_source}/CMakeLists.txt"
  "${add_subdirectory_source}/CMakeLists.txt.configured"
  @ONLY)
file(RENAME
  "${add_subdirectory_source}/CMakeLists.txt.configured"
  "${add_subdirectory_source}/CMakeLists.txt")

lexborpp_configure_build_test(
  "${add_subdirectory_source}" "${work_dir}/add-subdirectory-fake-config"
  "add_subdirectory fake-config consumer"
  "-Dlexbor_DIR=${fake_config_dir}"
  "-DLEXBORPP_FAKE_LEXBOR_INCLUDE_DIR=${LEXBORPP_LEXBOR_INCLUDE_DIR}"
  "-DLEXBORPP_FAKE_LEXBOR_LIBRARY=${LEXBORPP_REAL_LEXBOR_LIBRARY}"
  "-DLEXBORPP_FAKE_LEXBOR_COMPILE_DEFINITIONS=LEXBOR_STATIC"
  "-DLEXBORPP_EXPECT_LINKAGE=STATIC"
  "-DLEXBORPP_RUN_QUERY=ON")

lexborpp_configure_build_test(
  "${add_subdirectory_source}" "${work_dir}/add-subdirectory-raw-static"
  "add_subdirectory raw-static consumer"
  ${raw_explicit_args}
  "-DLEXBORPP_EXPECT_LINKAGE=STATIC")
