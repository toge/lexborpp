cmake_minimum_required(VERSION 3.25)
project(lexborpp VERSION 0.0.1 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(BUILD_TEST "Build tests" ON)
option(BUILD_BENCH "Build benchmarks" ON)

## add warning
if (CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g3")
endif()

if (MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
endif()

## optimization
if (CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")
endif()
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Ox")
endif()

## other flag
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(lexbor CONFIG QUIET)

# 1. lexbor::lexbor を確保する (vcpkg は lexbor::lexbor_static のみ提供するため補完)
if(NOT TARGET lexbor::lexbor)
    find_path(LEXBOR_INCLUDE_DIR NAMES lexbor/html/parser.h REQUIRED)
    find_library(LEXBOR_LIBRARY NAMES lexbor_static lexbor REQUIRED)

    add_library(lexbor::lexbor INTERFACE IMPORTED)
    target_include_directories(lexbor::lexbor INTERFACE "${LEXBOR_INCLUDE_DIR}")
    target_link_libraries(lexbor::lexbor INTERFACE "${LEXBOR_LIBRARY}")
endif()

# 2. LEXBOR_INCLUDE_DIR を必ず解決する (経路: find_path cache → target property → find_path)
#    VS generator では INTERFACE 伝搬が効かないことがあるため、各実ターゲットに
#    直接追加するために LEXBOR_INCLUDE_DIR が必要。
if(NOT LEXBOR_INCLUDE_DIR)
    if(TARGET lexbor::lexbor)
        get_target_property(_lexbor_inc_tmp lexbor::lexbor INTERFACE_INCLUDE_DIRECTORIES)
        if(_lexbor_inc_tmp AND NOT _lexbor_inc_tmp STREQUAL "_lexbor_inc_tmp-NOTFOUND")
            set(LEXBOR_INCLUDE_DIR "${_lexbor_inc_tmp}")
        endif()
    endif()
    if(NOT LEXBOR_INCLUDE_DIR)
        find_path(LEXBOR_INCLUDE_DIR NAMES lexbor/html/parser.h)
    endif()
endif()

add_library(lexborpp INTERFACE)
add_library(lexborpp::lexborpp ALIAS lexborpp)

target_include_directories(lexborpp
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(lexborpp INTERFACE lexbor::lexbor)
target_compile_features(lexborpp INTERFACE cxx_std_23)

set(LEXBORPP_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/lexborpp")

install(TARGETS lexborpp EXPORT lexborppTargets)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/lexborpp.hpp"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/lexborpp"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT lexborppTargets
        FILE lexborppTargets.cmake
        NAMESPACE lexborpp::
        DESTINATION ${LEXBORPP_INSTALL_CMAKEDIR})

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lexborppConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/lexborppConfig.cmake"
    INSTALL_DESTINATION ${LEXBORPP_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/lexborppConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/lexborppConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/lexborppConfigVersion.cmake"
        DESTINATION ${LEXBORPP_INSTALL_CMAKEDIR})

if(BUILD_TEST)
    add_subdirectory(test)
endif()

if(BUILD_BENCH)
    add_executable(bench_css_selector benchmark/bench_css_selector.cpp)
    target_link_libraries(bench_css_selector PRIVATE lexborpp::lexborpp)
    target_compile_features(bench_css_selector PRIVATE cxx_std_23)

    # VS generator fallback: lexbor include を直接追加 (INTERFACE IMPORTED 経由の伝搬が
    # 期待通り動作しない場合があるため)
    if(LEXBOR_INCLUDE_DIR)
        target_include_directories(bench_css_selector PRIVATE "${LEXBOR_INCLUDE_DIR}")
    endif()
endif()
