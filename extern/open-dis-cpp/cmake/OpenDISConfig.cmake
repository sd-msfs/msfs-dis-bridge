
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was OpenDISConfig.cmake.in                            ########

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

include(${CMAKE_CURRENT_LIST_DIR}/OpenDISTargets.cmake)

# Note: Since this file is executed each time users call
# find_dependency(OpenDIS), the target declarations are
# guarded to see if we already declared them

## Add target aliases
if (NOT TARGET OpenDIS::DIS6)
  add_library(OpenDIS::DIS6 ALIAS OpenDIS::OpenDIS6)
endif()
if (NOT TARGET OpenDIS::DIS7)
  add_library(OpenDIS::DIS7 ALIAS OpenDIS::OpenDIS7)
endif()

## These are deprecated target names. All new references to the targets
## should utilize the new namespace wrapped version
if (NOT TARGET OpenDIS6)
  add_library(OpenDIS6 ALIAS OpenDIS::OpenDIS6)
endif()
if (NOT TARGET OpenDIS7)
  add_library(OpenDIS7 ALIAS OpenDIS::OpenDIS7)
endif()
