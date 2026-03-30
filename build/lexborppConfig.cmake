
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was lexborppConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

include(CMakeFindDependencyMacro)

find_dependency(lexbor CONFIG QUIET)

if(NOT TARGET lexbor::lexbor)
    find_path(LEXBOR_INCLUDE_DIR NAMES lexbor/html/parser.h REQUIRED)
    find_library(LEXBOR_LIBRARY NAMES lexbor_static lexbor REQUIRED)

    add_library(lexbor::lexbor UNKNOWN IMPORTED)
    set_target_properties(lexbor::lexbor PROPERTIES
        IMPORTED_LOCATION "${LEXBOR_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LEXBOR_INCLUDE_DIR}"
    )
endif()

include("${CMAKE_CURRENT_LIST_DIR}/lexborppTargets.cmake")

check_required_components(lexborpp)
