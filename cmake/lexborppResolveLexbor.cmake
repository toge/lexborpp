function(lexborpp_resolve_lexbor_dependency out_prefix)
  find_package(lexbor CONFIG QUIET)

  set(found FALSE)
  set(link_item "")
  set(include_dirs "")
  set(compile_definitions "")
  set(concrete_library "")
  set(not_found_message "")

  if(TARGET lexbor::lexbor_static)
    set(found TRUE)
    set(link_item lexbor::lexbor_static)
    set(compile_definitions LEXBOR_STATIC)
    set(concrete_library "$<TARGET_FILE:lexbor::lexbor_static>")
  elseif(TARGET lexbor::lexbor)
    set(found TRUE)
    set(link_item lexbor::lexbor)
    get_target_property(lexbor_type lexbor::lexbor TYPE)
    if(NOT lexbor_type STREQUAL "INTERFACE_LIBRARY")
      set(concrete_library "$<TARGET_FILE:lexbor::lexbor>")
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
        if(LEXBORPP_RAW_LEXBOR_LINKAGE STREQUAL "SHARED")
          find_library(_LEXBORPP_LEXBOR_LIBRARY
            NAMES lexbor)
          set(raw_linkage_is_static FALSE)
        else()
          find_library(_LEXBORPP_LEXBOR_STATIC_LIBRARY
            NAMES lexbor_static)
          if(_LEXBORPP_LEXBOR_STATIC_LIBRARY)
            set(_LEXBORPP_LEXBOR_LIBRARY
              "${_LEXBORPP_LEXBOR_STATIC_LIBRARY}")
            set(raw_linkage_is_static TRUE)
          else()
            find_library(_LEXBORPP_LEXBOR_LIBRARY
              NAMES lexbor)
            if(LEXBORPP_RAW_LEXBOR_LINKAGE STREQUAL "STATIC")
              set(raw_linkage_is_static TRUE)
            else()
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
  set(${out_prefix}_NOT_FOUND_MESSAGE
    "${not_found_message}" PARENT_SCOPE)
endfunction()
