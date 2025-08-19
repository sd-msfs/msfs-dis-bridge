#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "OpenDIS::OpenDIS6" for configuration "Debug"
set_property(TARGET OpenDIS::OpenDIS6 APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(OpenDIS::OpenDIS6 PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/OpenDIS6.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/OpenDIS6.dll"
  )

list(APPEND _cmake_import_check_targets OpenDIS::OpenDIS6 )
list(APPEND _cmake_import_check_files_for_OpenDIS::OpenDIS6 "${_IMPORT_PREFIX}/lib/OpenDIS6.lib" "${_IMPORT_PREFIX}/bin/OpenDIS6.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
