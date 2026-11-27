# WASI_MINIMAL 向けスモークは常時ビルド可能（hosted 依存なし）
enable_language(C)

add_executable(smoke_wasi_minimal smoke_wasi_minimal.cpp)

target_link_libraries(smoke_wasi_minimal PRIVATE lexborpp::lexborpp)
target_include_directories(smoke_wasi_minimal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(LEXBOR_INCLUDE_DIR)
    target_include_directories(smoke_wasi_minimal PRIVATE "${LEXBOR_INCLUDE_DIR}")
endif()
target_compile_features(smoke_wasi_minimal PRIVATE ${LEXBORPP_CXX_STD})
target_compile_definitions(smoke_wasi_minimal PRIVATE LEXBORPP_WASI_MINIMAL=1)
# WASI_MINIMAL の契約 (例外なしでビルド可能) を hosted でも検証する
target_compile_options(smoke_wasi_minimal PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-exceptions>)
add_test(NAME smoke_wasi_minimal COMMAND smoke_wasi_minimal)

if(ENABLE_WASI_MINIMAL)
  # WASI_MINIMAL ビルドでは hosted なテスト/ベンチはスキップ
  # (Catch2 が wasip1 の signal 不足でビルド不可のため)
  return()
endif()

find_package(Catch2 REQUIRED CONFIG)

file(GLOB test_src CONFIGURE_DEPENDS test_*.cpp)
list(REMOVE_ITEM test_src ${CMAKE_CURRENT_SOURCE_DIR}/smoke_wasi_minimal.cpp ${CMAKE_CURRENT_SOURCE_DIR}/smoke_nttp.cpp)

add_executable(all_test ${test_src})

target_link_libraries(all_test PRIVATE Catch2::Catch2WithMain lexborpp::lexborpp)
target_include_directories(all_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(LEXBOR_INCLUDE_DIR)
    target_include_directories(all_test PRIVATE "${LEXBOR_INCLUDE_DIR}")
endif()
target_compile_features(all_test PRIVATE ${LEXBORPP_CXX_STD})

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    target_link_options(all_test PRIVATE "-sSTACK_SIZE=5242880")
endif()

# NTTP runtime smoke (例外あり・hosted のみ)
add_executable(smoke_nttp smoke_nttp.cpp)
target_link_libraries(smoke_nttp PRIVATE lexborpp::lexborpp)
target_include_directories(smoke_nttp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(LEXBOR_INCLUDE_DIR)
    target_include_directories(smoke_nttp PRIVATE "${LEXBOR_INCLUDE_DIR}")
endif()
target_compile_features(smoke_nttp PRIVATE ${LEXBORPP_CXX_STD})
add_test(NAME smoke_nttp COMMAND smoke_nttp)

add_test(NAME all_test COMMAND all_test -r junit)

if(NOT CMAKE_CROSSCOMPILING)
  string(REPLACE ";" "\\;" LEXBORPP_TEST_OSX_ARCHITECTURES
    "${CMAKE_OSX_ARCHITECTURES}")
  if(WIN32)
    set(LEXBORPP_SHARED_LINKER_SUFFIX_FOR_TEST
      "${CMAKE_IMPORT_LIBRARY_SUFFIX}")
  else()
    set(LEXBORPP_SHARED_LINKER_SUFFIX_FOR_TEST
      "${CMAKE_SHARED_LIBRARY_SUFFIX}")
  endif()
  add_test(
    NAME package_consumer_matrix
    COMMAND "${CMAKE_COMMAND}"
      "-DLEXBORPP_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
      "-DLEXBORPP_BINARY_DIR=${PROJECT_BINARY_DIR}"
      "-DLEXBORPP_TEST_CONFIG=$<CONFIG>"
      "-DLEXBORPP_REAL_LEXBOR_LIBRARY=${LEXBORPP_RESOLVED_CONCRETE_LIBRARY}"
      "-DLEXBORPP_REAL_LEXBOR_RUNTIME_LIBRARY=${LEXBORPP_RESOLVED_RUNTIME_LIBRARY}"
      "-DLEXBORPP_RESOLVED_LINKAGE=${LEXBORPP_RESOLVED_LINKAGE}"
      "-DLEXBORPP_LEXBOR_INCLUDE_DIR=${LEXBOR_INCLUDE_DIR}"
      "-DLEXBORPP_GENERATOR=${CMAKE_GENERATOR}"
      "-DLEXBORPP_GENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}"
      "-DLEXBORPP_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DLEXBORPP_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
      "-DLEXBORPP_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DLEXBORPP_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
      "-DLEXBORPP_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}"
      "-DLEXBORPP_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
      "-DLEXBORPP_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}"
      "-DLEXBORPP_OSX_ARCHITECTURES=${LEXBORPP_TEST_OSX_ARCHITECTURES}"
      "-DLEXBORPP_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
      "-DLEXBORPP_STATIC_LIBRARY_SUFFIX=${CMAKE_STATIC_LIBRARY_SUFFIX}"
      "-DLEXBORPP_SHARED_LIBRARY_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
      "-DLEXBORPP_IMPORT_LIBRARY_SUFFIX=${CMAKE_IMPORT_LIBRARY_SUFFIX}"
      "-DLEXBORPP_SHARED_LINKER_SUFFIX_FOR_TEST=${LEXBORPP_SHARED_LINKER_SUFFIX_FOR_TEST}"
      "-DLEXBORPP_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
      "-DLEXBORPP_VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}"
      "-DLEXBORPP_VCPKG_INSTALLED_DIR=${VCPKG_INSTALLED_DIR}"
      "-DLEXBORPP_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_consumer_test.cmake"
  )
endif()
