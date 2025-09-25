cmake_minimum_required(VERSION 3.10...3.29)
project(msfs-dis-bridge LANGUAGES CXX)

# Target Windows 10 APIs
add_definitions(-D_WIN32_WINNT=0x0A00)

# Make old subprojects (like Open-DIS) configure in at least 3.5 policy mode
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

# Define directory of MSFS 2024 SDK
set(MSFS_SDK_BASE_DIR "C:")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output folders
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ----------------------------------------------------------------------------
# Helper: create unique external build folder macro
# ----------------------------------------------------------------------------
macro(add_extern_subdir src_dir name)
    # src_dir: path to external source (relative or absolute)
    # name: short name used for a unique binary subfolder
    set(_src "${src_dir}")
    set(_bin "${CMAKE_BINARY_DIR}/extern-build/${name}")
    if(NOT EXISTS "${_src}/CMakeLists.txt")
        message(FATAL_ERROR "External subproject '${name}' missing CMakeLists.txt at ${_src}")
    endif()
    add_subdirectory("${_src}" "${_bin}")
endmacro()

# ----------------------------------------------------------------------------
# 1) SimConnect SDK (headers + libs must already be in extern/FlightSimSDK)
# ----------------------------------------------------------------------------
set(SIMCONNECT_INCLUDE_DIR "${MSFS_SDK_BASE_DIR}/MSFS 2024 SDK/SimConnect SDK/include")
set(SIMCONNECT_LIB_DIR     "${MSFS_SDK_BASE_DIR}/MSFS 2024 SDK/SimConnect SDK/lib")
include_directories(${SIMCONNECT_INCLUDE_DIR})

# ----------------------------------------------------------------------------
# 2) open-DIS (DIS 6) submodule - unique build dir
# ----------------------------------------------------------------------------
set(OPENDIS_SOURCE_DIR ${CMAKE_SOURCE_DIR}/extern/open-dis-cpp)
add_extern_subdir(${OPENDIS_SOURCE_DIR} open-dis)
include_directories(${OPENDIS_SOURCE_DIR}/src)

# ----------------------------------------------------------------------------
# 3) Asio (header-only) for Crow
# ----------------------------------------------------------------------------
set(ASIO_STANDALONE ON)
set(ASIO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/extern/asio/asio/include")
include_directories(${ASIO_INCLUDE_DIR})

# ----------------------------------------------------------------------------
# 4) Crow HTTP framework submodule
# ----------------------------------------------------------------------------
set(CROW_SOURCE_DIR ${CMAKE_SOURCE_DIR}/extern/crow)
# Crow might use Findasio.cmake
if(EXISTS "${CROW_SOURCE_DIR}/CMakeLists.txt")
    add_extern_subdir(${CROW_SOURCE_DIR} crow)
endif()
set(CROW_INCLUDE_DIR "${CROW_SOURCE_DIR}/include")
include_directories(${CROW_INCLUDE_DIR})

# ----------------------------------------------------------------------------
# 5) Geographic Lib (use a build dir under CMAKE_BINARY_DIR, not in source)
# ----------------------------------------------------------------------------
set(GEO_SOURCE_DIR ${CMAKE_SOURCE_DIR}/extern/GeographicLib-2.5)
set(GEO_BUILD_DIR ${CMAKE_SOURCE_DIR}/extern/GeographicLib-2.5/build)
set(GEO_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/extern/GeographicLib-2.5/include)
add_subdirectory(${GEO_SOURCE_DIR} ${GEO_BUILD_DIR})
include_directories(${GEO_INCLUDE_DIR})

# ----------------------------------------------------------------------------
# 6) yaml-cpp (submodule) - put its build output into extern-build/yaml-cpp
# ----------------------------------------------------------------------------
# disable yaml-cpp tests before adding
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests" FORCE)
set(YAMLCPP_SRC "${CMAKE_SOURCE_DIR}/extern/yaml-cpp")
add_extern_subdir(${YAMLCPP_SRC} yaml-cpp)

# ----------------------------------------------------------------------------
# 6b) tinyxml2 (submodule) - unique build dir for tinyxml2
# ----------------------------------------------------------------------------
# set(TINYXML2_SRC "${CMAKE_SOURCE_DIR}/extern/tinyxml2")
# add_extern_subdir(${TINYXML2_SRC} tinyxml2)

# ----------------------------------------------------------------------------
# 7) bridge-core library (static)
# ----------------------------------------------------------------------------
add_library(bridge-core STATIC
    src/bridge-core/Encode.cpp
    src/bridge-core/Decode.cpp
    src/bridge-core/MappingConfig.cpp
    src/bridge-core/PcapLogger.cpp
    src/bridge-manager/SimSession.cpp 
    src/bridge-manager/SessionManager.cpp 
)

target_include_directories(bridge-core PUBLIC
    ${CMAKE_SOURCE_DIR}/src/bridge-core
    ${CMAKE_SOURCE_DIR}/src/bridge-manager
    ${CROW_INCLUDE_DIR}
    ${ASIO_INCLUDE_DIR}
)
target_link_libraries(bridge-core PRIVATE 
    OpenDIS6
    yaml-cpp
#    tinyxml2
    GeographicLib
)

# ----------------------------------------------------------------------------
# 8) TestBridge executable
# ----------------------------------------------------------------------------
add_executable(TestBridge
    tests/pdu-replay/TestBridge.cpp
)

target_include_directories(TestBridge PRIVATE
    ${SIMCONNECT_INCLUDE_DIR}
    ${OPENDIS_SOURCE_DIR}/src
    ${GEO_INCLUDE_DIR}
)
target_link_libraries(TestBridge PRIVATE
    bridge-core
    OpenDIS6
    "${SIMCONNECT_LIB_DIR}/SimConnect.lib"
    yaml-cpp
#    tinyxml2
    GeographicLib
)

# ----------------------------------------------------------------------------
# 9) BridgeRunner executable
# ----------------------------------------------------------------------------
add_executable(BridgeRunner
    src/BridgeRunner.cpp
)

target_include_directories(BridgeRunner PRIVATE
    ${CMAKE_SOURCE_DIR}/src/bridge-core
    ${CMAKE_SOURCE_DIR}/src/bridge-manager
    ${SIMCONNECT_INCLUDE_DIR}
    ${OPENDIS_SOURCE_DIR}/src
)

target_compile_definitions(BridgeRunner PRIVATE
    ASIO_STANDALONE
    _WIN32_WINNT=0x0A00   # Target Windows 10 APIs
)

target_link_libraries(BridgeRunner PRIVATE
    bridge-core
    OpenDIS6
    "${SIMCONNECT_LIB_DIR}/SimConnect.lib"
)

# ----------------------------------------------------------------------------
# 10) REST-API server using Crow
# ----------------------------------------------------------------------------
file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/mappings" MAPPINGS_PATH)

add_executable(rest-api
    src/rest-api/main.cpp
)

target_include_directories(rest-api PRIVATE
    ${CMAKE_SOURCE_DIR}/src/bridge-core   
    ${SIMCONNECT_INCLUDE_DIR}              
    ${OPENDIS_SOURCE_DIR}/src             
    ${CROW_INCLUDE_DIR}                   
    ${ASIO_INCLUDE_DIR}
    ${GEO_INCLUDE_DIR}                   
)

target_compile_definitions(rest-api PRIVATE
    MAPPINGS_DIR="${MAPPINGS_PATH}"
    ASIO_STANDALONE
)

target_link_libraries(rest-api PRIVATE
    bridge-core
    OpenDIS6
    "${SIMCONNECT_LIB_DIR}/SimConnect.lib"
    yaml-cpp
#    tinyxml2
    GeographicLib
)

if (WIN32)
    add_custom_command(TARGET TestBridge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SIMCONNECT_LIB_DIR}/SimConnect.dll"   $<TARGET_FILE_DIR:TestBridge>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
             $<TARGET_FILE:OpenDIS6>                  $<TARGET_FILE_DIR:TestBridge>
        COMMENT "Copying required DLLs"
    )
endif()

# ----------------------------------------------------------------------------
# 11) Packaging (CPack ZIP with both binaries + DLLs + README)
# ----------------------------------------------------------------------------
include(InstallRequiredSystemLibraries)
install(TARGETS TestBridge rest-api RUNTIME DESTINATION bin)
install(FILES
    "$<TARGET_FILE:OpenDIS6>"
    "${SIMCONNECT_LIB_DIR}/SimConnect.dll"
    DESTINATION bin
)
install(FILES "${CMAKE_SOURCE_DIR}/README.md" DESTINATION .)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "msfs-dis-gateway-test")
set(CPACK_PACKAGE_VERSION "1.0.0")
include(CPack)

# ----------------------------------------------------------------------------
# 12) Config test executable
# ----------------------------------------------------------------------------
add_executable(msfs-host-script
    src/configs/msfs-host-script.cpp
)

target_link_libraries(msfs-host-script PRIVATE
    yaml-cpp
#    tinyxml2
)
