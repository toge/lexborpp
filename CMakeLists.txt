cmake_minimum_required(VERSION 3.25)
project(lexborpp VERSION 0.0.1 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

## add stdc++ compiler flag
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++26" COMPILER_SUPPORTS_CXX26)
CHECK_CXX_COMPILER_FLAG("-std=c++23" COMPILER_SUPPORTS_CXX23)
if(COMPILER_SUPPORTS_CXX26)
    set(STD_CPP cxx_std_26)
elseif(COMPILER_SUPPORTS_CXX23)
    set(STD_CPP cxx_std_23)
else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++23 support. Please use a different C++ compiler.")
endif()

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

find_package(lexbor CONFIG REQUIRED)

add_library(lexborpp INTERFACE)
add_library(lexborpp::lexborpp ALIAS lexborpp)

target_include_directories(lexborpp
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(lexborpp INTERFACE lexbor::lexbor)
target_compile_features(lexborpp INTERFACE ${STD_CPP})

set(LEXBORPP_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/lexborpp")

install(TARGETS lexborpp EXPORT lexborppTargets)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/lexborpp.hpp"
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

enable_testing()
add_subdirectory(test)
