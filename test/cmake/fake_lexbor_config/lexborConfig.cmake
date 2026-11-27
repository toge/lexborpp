if(NOT TARGET lexbor::lexbor)
  if(LEXBORPP_FAKE_LEXBOR_TARGET_KIND STREQUAL "SHARED")
    add_library(lexbor::lexbor SHARED IMPORTED)
    set_target_properties(lexbor::lexbor PROPERTIES
      IMPORTED_LOCATION "${LEXBORPP_FAKE_LEXBOR_RUNTIME_LIBRARY}")
    if(WIN32)
      set_target_properties(lexbor::lexbor PROPERTIES
        IMPORTED_IMPLIB "${LEXBORPP_FAKE_LEXBOR_LIBRARY}")
    endif()
  else()
    add_library(lexbor::lexbor INTERFACE IMPORTED)
  endif()
endif()

set_target_properties(lexbor::lexbor PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${LEXBORPP_FAKE_LEXBOR_INCLUDE_DIR}")
if(NOT LEXBORPP_FAKE_LEXBOR_TARGET_KIND STREQUAL "SHARED")
  set_target_properties(lexbor::lexbor PROPERTIES
    INTERFACE_LINK_LIBRARIES "${LEXBORPP_FAKE_LEXBOR_LIBRARY}")
endif()
set_target_properties(lexbor::lexbor PROPERTIES
  INTERFACE_COMPILE_DEFINITIONS
  "${LEXBORPP_FAKE_LEXBOR_COMPILE_DEFINITIONS}")
set(lexbor_FOUND TRUE)
