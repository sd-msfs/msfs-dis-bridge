cmake_minimum_required(VERSION 3.10...3.29)
project(msfs-dis-bridge LANGUAGES CXX)

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
# 1) SimConnect SDK (headers + libs must already be in extern/FlightSimSDK)
# ----------------------------------------------------------------------------
set(SIMCONNECT_INCLUDE_DIR "${MSFS_SDK_BASE_DIR}/MSFS 2024 SDK/SimConnect SDK/include")
set(SIMCONNECT_LIB_DIR     "${MSFS_SDK_BASE_DIR}/MSFS 2024 SDK/SimConnect SDK/lib")
include_directories(${SIMCONNECT_INCLUDE_DIR})

# ----------------------------------------------------------------------------
# 2) open‑DIS (DIS 6) submodule
# ----------------------------------------------------------------------------
set(OPENDIS_SOURCE_DIR ${CMAKE_SOURCE_DIR}/extern/open-dis-cpp)
set(OPENDIS_BUILD_DIR  ${CMAKE_BINARY_DIR}/open-dis-build)
add_subdirectory(${OPENDIS_SOURCE_DIR} ${OPENDIS_BUILD_DIR})
include_directories(${OPENDIS_SOURCE_DIR}/src)

# ----------------------------------------------------------------------------
# 3) Asio (header‑only) for Crow
# ----------------------------------------------------------------------------
set(ASIO_STANDALONE ON)
set(ASIO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/extern/asio/asio/include")

# ----------------------------------------------------------------------------
# 4) Crow HTTP framework submodule
# ----------------------------------------------------------------------------
set(CROW_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/extern/crow/include")

# ----------------------------------------------------------------------------
# 5) bridge-core library (static)
# ----------------------------------------------------------------------------
add_library(bridge-core STATIC
    src/bridge-core/Encode.cpp
    src/bridge-core/Decode.cpp
    src/bridge-core/MappingConfig.cpp
)
target_include_directories(bridge-core PUBLIC
    ${CMAKE_SOURCE_DIR}/src/bridge-core
)
target_link_libraries(bridge-core PRIVATE OpenDIS6)

# ----------------------------------------------------------------------------
# 6) TestBridge executable
# ----------------------------------------------------------------------------
add_executable(TestBridge
    tests/pdu-replay/TestBridge.cpp
)
target_include_directories(TestBridge PRIVATE
    ${SIMCONNECT_INCLUDE_DIR}
    ${OPENDIS_SOURCE_DIR}/src
)
target_link_libraries(TestBridge PRIVATE
    bridge-core
    OpenDIS6
    "${SIMCONNECT_LIB_DIR}/SimConnect.lib"
)

# ----------------------------------------------------------------------------
# 7) REST‑API server using Crow
# ----------------------------------------------------------------------------
add_executable(rest-api
    src/rest-api/main.cpp
)

target_include_directories(rest-api PRIVATE
    ${CMAKE_SOURCE_DIR}/src/bridge-core   
    ${SIMCONNECT_INCLUDE_DIR}              
    ${OPENDIS_SOURCE_DIR}/src             
    ${CROW_INCLUDE_DIR}                   
    ${ASIO_INCLUDE_DIR}                   
)

target_compile_definitions(rest-api PRIVATE
    ASIO_STANDALONE
)

target_link_libraries(rest-api PRIVATE
    bridge-core
    OpenDIS6
    "${SIMCONNECT_LIB_DIR}/SimConnect.lib"
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
# 8) Packaging (CPack ZIP with both binaries + DLLs + README)
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
