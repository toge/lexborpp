function(lexborpp_find_raw_library result_name linkage)
  unset(${result_name} CACHE)

  if(linkage STREQUAL "STATIC")
    set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_STATIC_LIBRARY_SUFFIX}")
  elseif(WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES "")
    if(CMAKE_IMPORT_LIBRARY_SUFFIX STREQUAL ".lib")
      list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".dll.lib")
    elseif(CMAKE_IMPORT_LIBRARY_SUFFIX STREQUAL ".a")
      list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".dll.a")
    endif()
    foreach(suffix IN LISTS CMAKE_FIND_LIBRARY_SUFFIXES_ORIGINAL)
      if(suffix MATCHES "\\.dll\\.(lib|a)$" OR
          suffix STREQUAL CMAKE_IMPORT_LIBRARY_SUFFIX)
        list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES "${suffix}")
      endif()
    endforeach()
    if(CMAKE_IMPORT_LIBRARY_SUFFIX AND
        NOT CMAKE_IMPORT_LIBRARY_SUFFIX IN_LIST CMAKE_FIND_LIBRARY_SUFFIXES)
      list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES
        "${CMAKE_IMPORT_LIBRARY_SUFFIX}")
    endif()
    if(NOT CMAKE_FIND_LIBRARY_SUFFIXES)
      set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_SHARED_LIBRARY_SUFFIX}")
    endif()
  else()
    set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_SHARED_LIBRARY_SUFFIX}")
  endif()

  find_library(${result_name} NAMES ${ARGN})
  set(${result_name} "${${result_name}}" PARENT_SCOPE)
endfunction()

function(lexborpp_resolve_lexbor_dependency out_prefix)
  find_package(lexbor CONFIG QUIET)

  set(found FALSE)
  set(link_item "")
  set(include_dirs "")
  set(compile_definitions "")
  set(concrete_library "")
  set(runtime_library "")
  set(linkage "UNKNOWN")
  set(not_found_message "")

  if(TARGET lexbor::lexbor_static)
    set(found TRUE)
    set(link_item lexbor::lexbor_static)
    set(compile_definitions LEXBOR_STATIC)
    get_target_property(lexbor_static_type lexbor::lexbor_static TYPE)
    if(NOT lexbor_static_type STREQUAL "INTERFACE_LIBRARY")
      set(concrete_library
        "$<TARGET_LINKER_FILE:lexbor::lexbor_static>")
    endif()
    set(linkage STATIC)
  elseif(TARGET lexbor::lexbor)
    set(found TRUE)
    set(link_item lexbor::lexbor)
    get_target_property(lexbor_type lexbor::lexbor TYPE)
    if(lexbor_type STREQUAL "STATIC_LIBRARY")
      set(concrete_library "$<TARGET_LINKER_FILE:lexbor::lexbor>")
      set(linkage STATIC)
    elseif(lexbor_type STREQUAL "UNKNOWN_LIBRARY")
      set(concrete_library "$<TARGET_LINKER_FILE:lexbor::lexbor>")
    elseif(lexbor_type STREQUAL "SHARED_LIBRARY" OR
        lexbor_type STREQUAL "MODULE_LIBRARY")
      set(concrete_library "$<TARGET_LINKER_FILE:lexbor::lexbor>")
      set(runtime_library "$<TARGET_FILE:lexbor::lexbor>")
      set(linkage SHARED)
    else()
      get_target_property(lexbor_definitions
        lexbor::lexbor INTERFACE_COMPILE_DEFINITIONS)
      if("LEXBOR_STATIC" IN_LIST lexbor_definitions)
        set(compile_definitions LEXBOR_STATIC)
        set(linkage STATIC)
      elseif(VCPKG_TARGET_TRIPLET MATCHES
          "(^|-)static($|-md$|-)")
        set(linkage STATIC)
      elseif(VCPKG_TARGET_TRIPLET)
        set(linkage SHARED)
      endif()
    endif()

    if(linkage STREQUAL "UNKNOWN" AND VCPKG_TARGET_TRIPLET
        MATCHES "(^|-)static($|-md$|-)")
      set(compile_definitions LEXBOR_STATIC)
      set(linkage STATIC)
    elseif(linkage STREQUAL "UNKNOWN" AND VCPKG_TARGET_TRIPLET)
      set(linkage SHARED)
    endif()
    if(linkage STREQUAL "STATIC")
      set(compile_definitions LEXBOR_STATIC)
    endif()
  else()
    unset(_LEXBORPP_LEXBOR_INCLUDE_DIR CACHE)
    unset(_LEXBORPP_LEXBOR_LIBRARY CACHE)
    unset(_LEXBORPP_LEXBOR_STATIC_LIBRARY CACHE)

    set(LEXBORPP_RAW_LEXBOR_LINKAGE "AUTO" CACHE STRING
      "Raw Lexbor linkage: AUTO, STATIC, or SHARED")
    set_property(CACHE LEXBORPP_RAW_LEXBOR_LINKAGE PROPERTY STRINGS
      AUTO STATIC SHARED)

    set(allowed_linkage_values AUTO STATIC SHARED)
    if(NOT LEXBORPP_RAW_LEXBOR_LINKAGE IN_LIST allowed_linkage_values)
      set(not_found_message
        "lexborpp could not resolve Lexbor: LEXBORPP_RAW_LEXBOR_LINKAGE must be exactly AUTO, STATIC, or SHARED (got '${LEXBORPP_RAW_LEXBOR_LINKAGE}')")
    endif()

    if(NOT not_found_message)
      find_path(_LEXBORPP_LEXBOR_INCLUDE_DIR
        NAMES lexbor/html/parser.h)
      if(NOT _LEXBORPP_LEXBOR_INCLUDE_DIR
          OR NOT EXISTS
            "${_LEXBORPP_LEXBOR_INCLUDE_DIR}/lexbor/html/parser.h")
        set(not_found_message
          "lexborpp could not find the Lexbor dependency: missing lexbor/html/parser.h")
      else()
        set(CMAKE_FIND_LIBRARY_SUFFIXES_ORIGINAL
          "${CMAKE_FIND_LIBRARY_SUFFIXES}")

        if(LEXBORPP_RAW_LEXBOR_LINKAGE STREQUAL "SHARED")
          lexborpp_find_raw_library(
            _LEXBORPP_LEXBOR_LIBRARY SHARED lexbor)
          set(raw_linkage_is_static FALSE)
        else()
          lexborpp_find_raw_library(
            _LEXBORPP_LEXBOR_STATIC_LIBRARY STATIC lexbor_static)
          if(_LEXBORPP_LEXBOR_STATIC_LIBRARY)
            set(_LEXBORPP_LEXBOR_LIBRARY
              "${_LEXBORPP_LEXBOR_STATIC_LIBRARY}")
            set(raw_linkage_is_static TRUE)
          else()
            if(LEXBORPP_RAW_LEXBOR_LINKAGE STREQUAL "STATIC")
              lexborpp_find_raw_library(
                _LEXBORPP_LEXBOR_LIBRARY STATIC lexbor)
              set(raw_linkage_is_static TRUE)
            else()
              lexborpp_find_raw_library(
                _LEXBORPP_LEXBOR_LIBRARY SHARED lexbor)
              set(raw_linkage_is_static FALSE)
            endif()
          endif()
        endif()

        if(_LEXBORPP_LEXBOR_LIBRARY)
          set(found TRUE)
          set(link_item "${_LEXBORPP_LEXBOR_LIBRARY}")
          set(include_dirs "${_LEXBORPP_LEXBOR_INCLUDE_DIR}")
          set(concrete_library "${_LEXBORPP_LEXBOR_LIBRARY}")
          if(raw_linkage_is_static)
            set(compile_definitions LEXBOR_STATIC)
            set(linkage STATIC)
          else()
            set(linkage SHARED)
          endif()
        elseif(LEXBORPP_RAW_LEXBOR_LINKAGE STREQUAL "SHARED")
          set(not_found_message
            "lexborpp could not find the Lexbor shared library 'lexbor' for LEXBORPP_RAW_LEXBOR_LINKAGE=SHARED")
        else()
          set(not_found_message
            "lexborpp could not find a raw Lexbor library named lexbor_static or lexbor for linkage mode '${LEXBORPP_RAW_LEXBOR_LINKAGE}'")
        endif()
      endif()
    endif()
  endif()

  set(${out_prefix}_FOUND "${found}" PARENT_SCOPE)
  set(${out_prefix}_LINK_ITEM "${link_item}" PARENT_SCOPE)
  set(${out_prefix}_INCLUDE_DIRS "${include_dirs}" PARENT_SCOPE)
  set(${out_prefix}_COMPILE_DEFINITIONS
    "${compile_definitions}" PARENT_SCOPE)
  set(${out_prefix}_CONCRETE_LIBRARY
    "${concrete_library}" PARENT_SCOPE)
  set(${out_prefix}_RUNTIME_LIBRARY
    "${runtime_library}" PARENT_SCOPE)
  set(${out_prefix}_LINKAGE
    "${linkage}" PARENT_SCOPE)
  set(${out_prefix}_NOT_FOUND_MESSAGE
    "${not_found_message}" PARENT_SCOPE)
endfunction()
