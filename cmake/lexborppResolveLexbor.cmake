function(lexborpp_validate_raw_shared_library
    out_valid out_runtime out_error candidate is_windows import_suffix
    static_suffix shared_suffix runtime_override)
  set(valid TRUE)
  set(runtime "")
  set(error "")

  if(runtime_override)
    if(IS_ABSOLUTE "${runtime_override}")
      set(runtime "${runtime_override}")
    else()
      cmake_path(ABSOLUTE_PATH runtime_override
        BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE
        OUTPUT_VARIABLE runtime)
    endif()

    if(NOT EXISTS "${runtime}")
      set(valid FALSE)
      set(error
        "Lexbor shared runtime library specified by LEXBORPP_RAW_LEXBOR_RUNTIME_LIBRARY does not exist: '${runtime}'")
    elseif(IS_DIRECTORY "${runtime}")
      set(valid FALSE)
      set(error
        "Lexbor shared runtime library specified by LEXBORPP_RAW_LEXBOR_RUNTIME_LIBRARY is not a regular file: '${runtime}'")
    else()
      set(runtime_name "${runtime}")
      set(expected_shared_suffix "${shared_suffix}")
      if(is_windows)
        string(TOLOWER "${runtime_name}" runtime_name)
        string(TOLOWER "${expected_shared_suffix}"
          expected_shared_suffix)
      endif()
      string(LENGTH "${expected_shared_suffix}" expected_suffix_length)
      string(LENGTH "${runtime_name}" runtime_name_length)
      if(runtime_name_length LESS expected_suffix_length)
        set(valid FALSE)
      else()
        math(EXPR runtime_tail_offset
          "${runtime_name_length} - ${expected_suffix_length}")
        string(SUBSTRING "${runtime_name}" "${runtime_tail_offset}" -1
          runtime_tail)
        if(NOT runtime_tail STREQUAL expected_shared_suffix)
          set(valid FALSE)
        endif()
      endif()
      if(NOT valid)
        set(error
          "Lexbor shared runtime library specified by LEXBORPP_RAW_LEXBOR_RUNTIME_LIBRARY must end with '${shared_suffix}': '${runtime}'")
      endif()
    endif()
    if(NOT valid)
      set(runtime "")
    endif()
  endif()

  if(valid AND is_windows)
    set(unambiguous FALSE)
    set(runtime_stem "")
    get_filename_component(candidate_name "${candidate}" NAME)
    string(TOLOWER "${candidate_name}" candidate_name_lower)

    foreach(suffix IN ITEMS ".dll.lib" ".dll.a")
      string(LENGTH "${suffix}" suffix_length)
      string(LENGTH "${candidate_name}" candidate_name_length)
      if(candidate_name_length GREATER_EQUAL suffix_length)
        math(EXPR runtime_stem_length
          "${candidate_name_length} - ${suffix_length}")
        string(SUBSTRING "${candidate_name_lower}"
          "${runtime_stem_length}" -1 candidate_tail)
        if(candidate_tail STREQUAL suffix)
          string(SUBSTRING "${candidate_name}" 0
            "${runtime_stem_length}" runtime_stem)
          set(unambiguous TRUE)
          break()
        endif()
      endif()
    endforeach()

    if(NOT unambiguous)
      set(ambiguous_suffix "${import_suffix}")
      foreach(suffix IN ITEMS
          "${shared_suffix}" "${import_suffix}" "${static_suffix}")
        if(NOT suffix)
          continue()
        endif()
        string(TOLOWER "${suffix}" suffix_lower)
        string(LENGTH "${suffix}" suffix_length)
        string(LENGTH "${candidate_name}" candidate_name_length)
        if(candidate_name_length GREATER_EQUAL suffix_length)
          math(EXPR runtime_stem_length
            "${candidate_name_length} - ${suffix_length}")
          string(SUBSTRING "${candidate_name_lower}"
            "${runtime_stem_length}" -1 candidate_tail)
          if(candidate_tail STREQUAL suffix_lower)
            string(SUBSTRING "${candidate_name}" 0
              "${runtime_stem_length}" runtime_stem)
            set(ambiguous_suffix "${suffix}")
            if(suffix STREQUAL shared_suffix)
              set(unambiguous TRUE)
              set(runtime "${candidate}")
            endif()
            break()
          endif()
        endif()
      endforeach()
    endif()

    if(NOT runtime AND runtime_stem)
      get_filename_component(candidate_dir "${candidate}" DIRECTORY)
      get_filename_component(candidate_parent
        "${candidate_dir}" DIRECTORY)
      foreach(runtime_dir
          "${candidate_dir}"
          "${candidate_parent}/bin")
        set(runtime_candidate
          "${runtime_dir}/${runtime_stem}${shared_suffix}")
        if(EXISTS "${runtime_candidate}" AND
            NOT IS_DIRECTORY "${runtime_candidate}")
          cmake_path(NORMAL_PATH runtime_candidate
            OUTPUT_VARIABLE runtime)
          break()
        endif()
      endforeach()
    endif()

    if(valid AND NOT runtime AND NOT unambiguous)
      set(valid FALSE)
      set(error
        "ambiguous ${ambiguous_suffix} without matching runtime library: '${candidate}'")
    endif()
  endif()

  set(${out_valid} "${valid}" PARENT_SCOPE)
  set(${out_runtime} "${runtime}" PARENT_SCOPE)
  set(${out_error} "${error}" PARENT_SCOPE)
endfunction()

function(lexborpp_find_raw_library result_name linkage)
  unset(${result_name} CACHE)

  if(linkage STREQUAL "STATIC")
    set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_STATIC_LIBRARY_SUFFIX}")
  elseif(WIN32)
    set(unambiguous_result_name
      "${result_name}_UNAMBIGUOUS")
    if(CMAKE_IMPORT_LIBRARY_SUFFIX STREQUAL ".dll.a" OR
        CMAKE_IMPORT_LIBRARY_SUFFIX STREQUAL ".a")
      set(unambiguous_suffixes ".dll.a" ".dll.lib")
    else()
      set(unambiguous_suffixes ".dll.lib" ".dll.a")
    endif()
    foreach(unambiguous_suffix IN LISTS unambiguous_suffixes)
      unset(${unambiguous_result_name})
      unset(${unambiguous_result_name} CACHE)
      set(CMAKE_FIND_LIBRARY_SUFFIXES "${unambiguous_suffix}")
      find_library(${unambiguous_result_name} NAMES ${ARGN} NO_CACHE)
      if(${unambiguous_result_name})
        set(${result_name}
          "${${unambiguous_result_name}}" PARENT_SCOPE)
        return()
      endif()
    endforeach()
    unset(${unambiguous_result_name})
    unset(${unambiguous_result_name} CACHE)
    unset(${result_name})
    unset(${result_name} CACHE)
    set(CMAKE_FIND_LIBRARY_SUFFIXES
      "${CMAKE_IMPORT_LIBRARY_SUFFIX}"
      "${CMAKE_STATIC_LIBRARY_SUFFIX}")
  else()
    set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_SHARED_LIBRARY_SUFFIX}")
  endif()

  find_library(${result_name} NAMES ${ARGN} NO_CACHE)
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
    unset(_LEXBORPP_LEXBOR_INCLUDE_DIR)
    unset(_LEXBORPP_LEXBOR_INCLUDE_DIR CACHE)
    unset(_LEXBORPP_LEXBOR_LIBRARY)
    unset(_LEXBORPP_LEXBOR_LIBRARY CACHE)
    unset(_LEXBORPP_LEXBOR_STATIC_LIBRARY)
    unset(_LEXBORPP_LEXBOR_STATIC_LIBRARY CACHE)

    set(LEXBORPP_RAW_LEXBOR_LINKAGE "AUTO" CACHE STRING
      "Raw Lexbor linkage: AUTO, STATIC, or SHARED")
    set_property(CACHE LEXBORPP_RAW_LEXBOR_LINKAGE PROPERTY STRINGS
      AUTO STATIC SHARED)
    set(LEXBORPP_RAW_LEXBOR_RUNTIME_LIBRARY "" CACHE FILEPATH
      "Lexbor shared runtime library used to validate raw shared linkage")

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
          if(raw_linkage_is_static)
            set(found TRUE)
            set(compile_definitions LEXBOR_STATIC)
            set(linkage STATIC)
          else()
            lexborpp_validate_raw_shared_library(
              raw_shared_valid raw_shared_runtime raw_shared_error
              "${_LEXBORPP_LEXBOR_LIBRARY}" "${WIN32}"
              "${CMAKE_IMPORT_LIBRARY_SUFFIX}"
              "${CMAKE_STATIC_LIBRARY_SUFFIX}"
              "${CMAKE_SHARED_LIBRARY_SUFFIX}"
              "${LEXBORPP_RAW_LEXBOR_RUNTIME_LIBRARY}")
            if(raw_shared_valid)
              set(found TRUE)
              set(linkage SHARED)
              set(runtime_library "${raw_shared_runtime}")
            else()
              set(not_found_message
                "lexborpp could not resolve the Lexbor shared library: ${raw_shared_error}")
            endif()
          endif()
          if(found)
            set(link_item "${_LEXBORPP_LEXBOR_LIBRARY}")
            set(include_dirs "${_LEXBORPP_LEXBOR_INCLUDE_DIR}")
            set(concrete_library "${_LEXBORPP_LEXBOR_LIBRARY}")
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
