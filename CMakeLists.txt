set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Min policy ver for subprojects")
cmake_minimum_required(VERSION 3.10...3.29)
project(msfs-dis-bridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows 10 APIs
add_definitions(-D_WIN32_WINNT=0x0A00)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ----------------------------------------------------------------------------
# 1) SimConnect SDK
# ----------------------------------------------------------------------------
set(MSFS_SDK_BASE_DIR "C:")  # adjust
set(SIMCONNECT_INCLUDE_DIR "${MSFS_SDK_BASE_DIR}/MSFS 2024 SDK/SimConnect SDK/include")
set(SIMCONNECT_LIB_DIR     "${MSFS_SDK_BASE_DIR}/MSFS 2024 SDK/SimConnect SDK/lib")
include_directories(${SIMCONNECT_INCLUDE_DIR})

# ----------------------------------------------------------------------------
# 2) Open-DIS
# ----------------------------------------------------------------------------
set(OPENDIS_SOURCE_DIR ${CMAKE_SOURCE_DIR}/extern/open-dis-cpp)
set(OPENDIS_BUILD_DIR  ${CMAKE_BINARY_DIR}/open-dis-build)
add_subdirectory(${OPENDIS_SOURCE_DIR} ${OPENDIS_BUILD_DIR})
include_directories(${OPENDIS_SOURCE_DIR}/src)

# ----------------------------------------------------------------------------
# 3) Asio + Crow (header only)
# ----------------------------------------------------------------------------
add_definitions(-D_WIN32_WINNT=0x0A00)
set(ASIO_STANDALONE ON)
set(ASIO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/extern/asio/asio/include")
set(CROW_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/extern/crow/include")

# ----------------------------------------------------------------------------
# 4) Core library (bridge-core)
# ----------------------------------------------------------------------------
add_library(bridge-core STATIC
    src/bridge-core/Encode.cpp
    src/bridge-core/Decode.cpp
    src/bridge-core/MappingConfig.cpp
    src/bridge-manager/SimSession.cpp
    src/bridge-manager/SessionManager.cpp
    src/bridge-multicast/UDPMulticaster.cpp
)

target_include_directories(bridge-core PUBLIC
    ${CMAKE_SOURCE_DIR}/src/bridge-core
    ${CMAKE_SOURCE_DIR}/src/bridge-manager
    ${CMAKE_SOURCE_DIR}/src/bridge-multicast
    ${CROW_INCLUDE_DIR}
    ${ASIO_INCLUDE_DIR}
    ${SIMCONNECT_INCLUDE_DIR}
    ${OPENDIS_SOURCE_DIR}/src
)

target_compile_definitions(bridge-core PUBLIC
    ASIO_STANDALONE
    _WIN32_WINNT=0x0A00
)

target_link_libraries(bridge-core PUBLIC OpenDIS6)

if (WIN32)
    target_link_libraries(bridge-core PUBLIC ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(bridge-core PUBLIC Threads::Threads)
endif()

# ----------------------------------------------------------------------------
# 5) TestBridge executable
# ----------------------------------------------------------------------------
add_executable(TestBridge tests/pdu-replay/TestBridge.cpp)
target_link_libraries(TestBridge PRIVATE bridge-core "${SIMCONNECT_LIB_DIR}/SimConnect.lib")

# ----------------------------------------------------------------------------
# 6) BridgeRunner executable
# ----------------------------------------------------------------------------
add_executable(BridgeRunner
    src/BridgeRunner.cpp
    src/rest-api/RestServer.cpp
)

# Make sure BridgeRunner can see the header
target_include_directories(BridgeRunner PRIVATE
    ${CMAKE_SOURCE_DIR}/src/rest-api
    ${CROW_INCLUDE_DIR}
    ${ASIO_INCLUDE_DIR}
)

target_link_libraries(BridgeRunner PRIVATE bridge-core "${SIMCONNECT_LIB_DIR}/SimConnect.lib")

# ----------------------------------------------------------------------------
# 7) REST-API server (Crow)
# ----------------------------------------------------------------------------
file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/mappings" MAPPINGS_PATH)

add_executable(rest-api src/rest-api/main.cpp)
target_include_directories(rest-api PRIVATE ${CROW_INCLUDE_DIR} ${ASIO_INCLUDE_DIR})
target_compile_definitions(rest-api PRIVATE MAPPINGS_DIR="${MAPPINGS_PATH}" ASIO_STANDALONE)
target_link_libraries(rest-api PRIVATE bridge-core "${SIMCONNECT_LIB_DIR}/SimConnect.lib")

# ----------------------------------------------------------------------------
# 8) Post-build copy of DLLs (Windows)
# ----------------------------------------------------------------------------
if (WIN32)
    add_custom_command(TARGET TestBridge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SIMCONNECT_LIB_DIR}/SimConnect.dll" $<TARGET_FILE_DIR:TestBridge>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:OpenDIS6> $<TARGET_FILE_DIR:TestBridge>
    )
endif()

# ----------------------------------------------------------------------------
# 9) Packaging
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
