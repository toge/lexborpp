cmake_minimum_required(VERSION 3.25...3.31)
project(fake_lexbor_raw LANGUAGES C)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(FAKE_LEXBOR_KIND STREQUAL "STATIC_EXPLICIT")
  add_library(lexbor_static STATIC fake.c)
  set(fake_target lexbor_static)
elseif(FAKE_LEXBOR_KIND STREQUAL "STATIC_GENERIC")
  add_library(lexbor STATIC fake.c)
  set(fake_target lexbor)
elseif(FAKE_LEXBOR_KIND STREQUAL "SHARED_GENERIC")
  add_library(lexbor SHARED fake.c)
  set(fake_target lexbor)
else()
  message(FATAL_ERROR "FAKE_LEXBOR_KIND is invalid")
endif()

target_include_directories(${fake_target} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
if(FAKE_LEXBOR_KIND STREQUAL "SHARED_GENERIC")
  target_compile_definitions(${fake_target}
    PRIVATE FAKE_LEXBOR_BUILDING_SHARED=1)
  if(WIN32)
    set_target_properties(${fake_target} PROPERTIES
      RUNTIME_OUTPUT_NAME lexbor
      ARCHIVE_OUTPUT_NAME lexbor.dll)
  endif()
endif()

install(TARGETS ${fake_target}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
install(FILES fake.h DESTINATION probe/include)
