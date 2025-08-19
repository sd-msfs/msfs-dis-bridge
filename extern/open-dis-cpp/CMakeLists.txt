cmake_minimum_required(VERSION 3.2)
project(OpenDIS VERSION 1.1.0)

# Build options - removed BUILD_EXAMPLES and BUILD_TESTS since directories are pruned
option(BUILD_SHARED_LIBS "build shared libraries" ON)

# Enforce a C++11 compliant compiler
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include GNUInstallDirs Module
include(GNUInstallDirs)

# Define only OpenDIS6 Library
add_library(OpenDIS6 "")

# Add src/ to the include directories
target_include_directories(OpenDIS6
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# Windows-specific linking
if(WIN32)
target_link_libraries(OpenDIS6 PRIVATE ws2_32)
endif()

# Add source directories (only DIS6 remains)
add_subdirectory(src)
add_subdirectory(cmake)

# Removed BUILD_EXAMPLES and BUILD_TESTS sections since directories are pruned

# Configure install target - only OpenDIS6 remains
install(TARGETS OpenDIS6
        EXPORT OpenDISTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
        
install(EXPORT OpenDISTargets
        NAMESPACE OpenDIS::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/OpenDIS")

# Configure package target
set(CPACK_GENERATOR "DEB" "RPM" "TXZ" "TGZ")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_NAME "OpenDis")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "The open DIS cpp library")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_RPM_DIR "${CMAKE_CURRENT_BINARY_DIR}/_CPack_Packages/Linux/RPM")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Leif Gruenwoldt <leifer@gmail.com>")
set(CPACK_PACKAGE_PACKAGER $ENV{USER})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "The open DIS cpp library")
include(CPack)