# WASI_MINIMAL 向けスモークは常時ビルド可能（hosted 依存なし）
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
