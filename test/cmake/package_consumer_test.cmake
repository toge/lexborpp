cmake_minimum_required(VERSION 3.25...3.31)

set(required_variables
  LEXBORPP_SOURCE_DIR
  LEXBORPP_BINARY_DIR
  LEXBORPP_TEST_CONFIG
  LEXBORPP_LEXBOR_INCLUDE_DIR
  LEXBORPP_RESOLVED_LINKAGE
  LEXBORPP_GENERATOR
  LEXBORPP_C_COMPILER
  LEXBORPP_CXX_COMPILER
  LEXBORPP_SHARED_LINKER_SUFFIX_FOR_TEST
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
    string(REPLACE "\\;" ";" osx_architectures
      "${LEXBORPP_OSX_ARCHITECTURES}")
    string(REPLACE ";" "\\;" escaped_osx_architectures
      "${osx_architectures}")
    list(APPEND args
      "-DCMAKE_OSX_ARCHITECTURES=${escaped_osx_architectures}")
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

if(NOT LEXBORPP_RESOLVED_LINKAGE STREQUAL "STATIC" AND
    NOT LEXBORPP_RESOLVED_LINKAGE STREQUAL "SHARED")
  message(FATAL_ERROR
    "cannot derive vcpkg Lexbor linkage from target/triplet: "
    "'${LEXBORPP_RESOLVED_LINKAGE}'")
endif()

set(vcpkg_runtime_args)
if(LEXBORPP_RESOLVED_LINKAGE STREQUAL "SHARED" AND
    LEXBORPP_REAL_LEXBOR_RUNTIME_LIBRARY)
  get_filename_component(vcpkg_runtime_dir
    "${LEXBORPP_REAL_LEXBOR_RUNTIME_LIBRARY}" DIRECTORY)
  list(APPEND vcpkg_runtime_args
    "-DLEXBORPP_FAKE_LEXBOR_RUNTIME_DIR=${vcpkg_runtime_dir}")
endif()

lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-vcpkg"
  "installed vcpkg ${LEXBORPP_RESOLVED_LINKAGE} consumer"
  "-Dlexborpp_DIR=${lexborpp_config_dir}"
  "-DLEXBORPP_EXPECT_LINKAGE=${LEXBORPP_RESOLVED_LINKAGE}"
  "-DLEXBORPP_RUN_QUERY=ON"
  ${vcpkg_runtime_args}
  ${config_toolchain_args})

if(LEXBORPP_RESOLVED_LINKAGE STREQUAL "STATIC")
elseif(LEXBORPP_VCPKG_TARGET_TRIPLET AND
    LEXBORPP_VCPKG_INSTALLED_DIR)
  if(LEXBORPP_VCPKG_TARGET_TRIPLET MATCHES "^(.+-)(windows|osx|linux)$")
    set(static_triplet
      "${CMAKE_MATCH_1}${CMAKE_MATCH_2}-static")
  elseif(LEXBORPP_VCPKG_TARGET_TRIPLET MATCHES "^(.+)$")
    set(static_triplet "${CMAKE_MATCH_1}-static")
  endif()
  if(EXISTS
      "${LEXBORPP_VCPKG_INSTALLED_DIR}/${static_triplet}/share/lexbor/lexborConfig.cmake")
    lexborpp_configure_build_test(
      "${consumer_source}" "${work_dir}/consumer-vcpkg-static"
      "installed vcpkg STATIC consumer"
      "-Dlexborpp_DIR=${lexborpp_config_dir}"
      "-DLEXBORPP_EXPECT_LINKAGE=STATIC"
      "-DLEXBORPP_RUN_QUERY=ON"
      ${config_toolchain_args}
      "-DVCPKG_TARGET_TRIPLET=${static_triplet}")
  endif()
endif()

set(fake_static_explicit_prefix "${work_dir}/fake-static-explicit")
set(fake_generic_combined_prefix "${work_dir}/fake-generic-combined")
set(fake_static_only_prefix "${work_dir}/fake-static-only")
set(fake_shared_only_prefix "${work_dir}/fake-shared-only")

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
  STATIC_GENERIC "${fake_generic_combined_prefix}"
  "fake-generic-combined-static")
lexborpp_build_fake(
  SHARED_GENERIC "${fake_generic_combined_prefix}"
  "fake-generic-combined-shared")
lexborpp_build_fake(
  STATIC_GENERIC "${fake_static_only_prefix}" "fake-static-only")
lexborpp_build_fake(
  SHARED_GENERIC "${fake_shared_only_prefix}" "fake-shared-only")

file(GLOB fake_generic_static_libraries
  "${fake_generic_combined_prefix}/lib/*lexbor${LEXBORPP_STATIC_LIBRARY_SUFFIX}")
if(WIN32)
  list(FILTER fake_generic_static_libraries
    EXCLUDE REGEX "\\.dll\\.(lib|a)$")
endif()
list(LENGTH fake_generic_static_libraries
  fake_generic_static_libraries_count)
if(NOT fake_generic_static_libraries_count EQUAL 1)
  message(FATAL_ERROR
    "expected one fake generic static library, found "
    "${fake_generic_static_libraries}")
endif()
list(GET fake_generic_static_libraries 0 fake_generic_static_library)

if(WIN32)
  file(GLOB fake_generic_shared_runtime_libraries
    "${fake_generic_combined_prefix}/bin/*lexbor${LEXBORPP_SHARED_LIBRARY_SUFFIX}")
  file(GLOB fake_generic_shared_link_libraries
    "${fake_generic_combined_prefix}/lib/*lexbor.dll${LEXBORPP_IMPORT_LIBRARY_SUFFIX}")
else()
  file(GLOB fake_generic_shared_runtime_libraries
    "${fake_generic_combined_prefix}/lib/*lexbor${LEXBORPP_SHARED_LIBRARY_SUFFIX}")
  set(fake_generic_shared_link_libraries
    "${fake_generic_shared_runtime_libraries}")
endif()
foreach(fake_shared_file_type IN ITEMS runtime link)
  list(LENGTH fake_generic_shared_${fake_shared_file_type}_libraries
    fake_generic_shared_${fake_shared_file_type}_libraries_count)
  if(NOT fake_generic_shared_${fake_shared_file_type}_libraries_count EQUAL 1)
    message(FATAL_ERROR
      "expected one fake shared ${fake_shared_file_type} library, found "
      "${fake_generic_shared_${fake_shared_file_type}_libraries}")
  endif()
endforeach()
list(GET fake_generic_shared_runtime_libraries 0
  fake_generic_shared_runtime_library)
list(GET fake_generic_shared_link_libraries 0
  fake_generic_shared_link_library)
get_filename_component(fake_shared_runtime_dir
  "${fake_generic_shared_runtime_library}" DIRECTORY)

lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-fake-config"
  "installed fake-config consumer"
  "-Dlexborpp_DIR=${lexborpp_config_dir}"
  "-Dlexbor_DIR=${fake_config_dir}"
  "-DLEXBORPP_FAKE_LEXBOR_INCLUDE_DIR=${LEXBORPP_LEXBOR_INCLUDE_DIR}"
  "-DLEXBORPP_FAKE_LEXBOR_LIBRARY=${fake_generic_static_library}"
  "-DLEXBORPP_FAKE_LEXBOR_COMPILE_DEFINITIONS=LEXBOR_STATIC"
  "-DLEXBORPP_FAKE_LEXBOR_PROBE_INCLUDE_DIR=${fake_generic_combined_prefix}/probe/include"
  "-DLEXBORPP_EXPECT_LINKAGE=STATIC"
  "-DLEXBORPP_LINK_PROBE=ON"
  "-DLEXBORPP_EXPECT_INTERFACE_ONLY=ON")

lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-fake-shared-config"
  "installed fake shared-target config consumer"
  "-Dlexborpp_DIR=${lexborpp_config_dir}"
  "-Dlexbor_DIR=${fake_config_dir}"
  "-DLEXBORPP_FAKE_LEXBOR_TARGET_KIND=SHARED"
  "-DLEXBORPP_FAKE_LEXBOR_INCLUDE_DIR=${LEXBORPP_LEXBOR_INCLUDE_DIR}"
  "-DLEXBORPP_FAKE_LEXBOR_LIBRARY=${fake_generic_shared_link_library}"
  "-DLEXBORPP_FAKE_LEXBOR_RUNTIME_LIBRARY=${fake_generic_shared_runtime_library}"
  "-DLEXBORPP_FAKE_LEXBOR_PROBE_INCLUDE_DIR=${fake_generic_combined_prefix}/probe/include"
  "-DLEXBORPP_EXPECT_LINKAGE=SHARED"
  "-DLEXBORPP_LINK_PROBE=ON"
  "-DLEXBORPP_FAKE_LEXBOR_RUNTIME_DIR=${fake_shared_runtime_dir}"
  "-DLEXBORPP_EXPECT_RESOLVED_TARGET=lexbor::lexbor")

if(LEXBORPP_SYSTEM_NAME STREQUAL "Windows")
  set(raw_shared_runtime_dir "${fake_generic_combined_prefix}/bin")
else()
  set(raw_shared_runtime_dir "${fake_generic_combined_prefix}/lib")
endif()

function(lexborpp_raw_consumer_args prefix result)
  set(args
    ${raw_isolation_args}
    "-Dlexborpp_DIR=${lexborpp_config_dir}"
    "-DCMAKE_INCLUDE_PATH=${prefix}/include"
    "-DCMAKE_LIBRARY_PATH=${prefix}/lib"
    "-DLEXBORPP_LINK_PROBE=ON"
    "-DLEXBORPP_FAKE_LEXBOR_PROBE_INCLUDE_DIR=${prefix}/probe/include")
  set(${result} "${args}" PARENT_SCOPE)
endfunction()

lexborpp_raw_consumer_args(
  "${fake_static_explicit_prefix}"
  raw_explicit_args)
lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-raw-explicit-auto"
  "raw explicit static AUTO consumer"
  ${raw_explicit_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=AUTO"
  "-DLEXBORPP_EXPECT_LINKAGE=STATIC")

lexborpp_raw_consumer_args(
  "${fake_generic_combined_prefix}"
  raw_combined_args)
lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-raw-generic-static"
  "raw generic STATIC consumer"
  ${raw_combined_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=STATIC"
  "-DLEXBORPP_EXPECT_LINKAGE=STATIC"
  "-DLEXBORPP_EXPECT_RESOLVED_LIBRARY_SUFFIX=${LEXBORPP_STATIC_LIBRARY_SUFFIX}")

lexborpp_raw_consumer_args(
  "${fake_static_only_prefix}"
  raw_static_only_args)
lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-raw-static-only"
  "raw generic static-only consumer"
  ${raw_static_only_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=STATIC"
  "-DLEXBORPP_EXPECT_LINKAGE=STATIC"
  "-DLEXBORPP_EXPECT_RESOLVED_LIBRARY_SUFFIX=${LEXBORPP_STATIC_LIBRARY_SUFFIX}")

lexborpp_raw_consumer_args(
  "${fake_shared_only_prefix}"
  raw_shared_only_args)

lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-raw-generic-shared"
  "raw generic SHARED consumer"
  ${raw_combined_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=SHARED"
  "-DLEXBORPP_EXPECT_LINKAGE=SHARED"
  "-DLEXBORPP_FAKE_LEXBOR_RUNTIME_DIR=${raw_shared_runtime_dir}"
  "-DLEXBORPP_EXPECT_RESOLVED_LIBRARY_SUFFIX=${LEXBORPP_SHARED_LINKER_SUFFIX_FOR_TEST}")

lexborpp_configure_build_test(
  "${consumer_source}" "${work_dir}/consumer-raw-generic-auto-shared"
  "raw generic AUTO-as-shared consumer"
  ${raw_combined_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=AUTO"
  "-DLEXBORPP_EXPECT_LINKAGE=SHARED"
  "-DLEXBORPP_FAKE_LEXBOR_RUNTIME_DIR=${raw_shared_runtime_dir}"
  "-DLEXBORPP_EXPECT_RESOLVED_LIBRARY_SUFFIX=${LEXBORPP_SHARED_LINKER_SUFFIX_FOR_TEST}")

lexborpp_require_configure_failure(
  "${consumer_source}" "${work_dir}/consumer-invalid-linkage"
  "invalid raw linkage" "LEXBORPP_RAW_LEXBOR_LINKAGE"
  ${raw_combined_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=dynamic")

lexborpp_require_configure_failure(
  "${consumer_source}" "${work_dir}/consumer-shared-only-static"
  "SHARED with only explicit static artifact"
  "lexbor.*shared|library.*lexbor"
  ${raw_explicit_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=SHARED")

lexborpp_require_configure_failure(
  "${consumer_source}" "${work_dir}/consumer-static-only-shared-artifact"
  "STATIC with only shared generic artifact"
  "Lexbor.*static|library.*static"
  ${raw_shared_only_args}
  "-DLEXBORPP_RAW_LEXBOR_LINKAGE=STATIC")

if(NOT LEXBORPP_SYSTEM_NAME STREQUAL "Windows")
  lexborpp_require_configure_failure(
    "${consumer_source}" "${work_dir}/consumer-shared-only-static-artifact"
    "SHARED with only static generic artifact"
    "Lexbor.*shared|library.*lexbor"
    ${raw_static_only_args}
    "-DLEXBORPP_RAW_LEXBOR_LINKAGE=SHARED")
endif()

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
    "${LEXBORPP_FAKE_LEXBOR_PROBE_INCLUDE_DIR}")
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
  "-DLEXBORPP_FAKE_LEXBOR_LIBRARY=${fake_generic_static_library}"
  "-DLEXBORPP_FAKE_LEXBOR_COMPILE_DEFINITIONS=LEXBOR_STATIC"
  "-DLEXBORPP_FAKE_LEXBOR_PROBE_INCLUDE_DIR=${fake_generic_combined_prefix}/probe/include"
  "-DLEXBORPP_EXPECT_LINKAGE=STATIC"
  "-DLEXBORPP_LINK_PROBE=ON")

lexborpp_configure_build_test(
  "${add_subdirectory_source}" "${work_dir}/add-subdirectory-raw-static"
  "add_subdirectory raw-static consumer"
  ${raw_explicit_args}
  "-DLEXBORPP_EXPECT_LINKAGE=STATIC")
