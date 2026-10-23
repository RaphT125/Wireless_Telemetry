cmake_minimum_required(VERSION 3.21)

# Let CLion / command-line builds find vcpkg packages (needs the VCPKG_ROOT
# environment variable). Must be set before project().
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()

project(Wireless_Telemetry CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(asio CONFIG REQUIRED)

# Core logic: UDP receiver + service wrapper.
add_library(telemetry_core STATIC
    DesktopReciverService/Receiver.cpp
    DesktopReciverService/TelemetryService.cpp
        DesktopReciverService/PacketHandler.cpp
        DesktopReciverService/PacketHandler.h
        DesktopReciverService/CANDeserializer.cpp
        DesktopReciverService/CANDeserializer.h
        DesktopReciverService/ProcessedCanData.h
        DesktopReciverService/CANMap.h
        DesktopReciverService/Tunneling.cpp
        DesktopReciverService/Tunneling.h
        DesktopReciverService/iParser.h
        DesktopReciverService/JsonParser.cpp
        DesktopReciverService/JsonParser.h
)
target_include_directories(telemetry_core PUBLIC DesktopReciverService)
target_link_libraries(telemetry_core PUBLIC asio::asio)
target_compile_definitions(telemetry_core PUBLIC ASIO_STANDALONE)

if(WIN32)
    target_compile_definitions(telemetry_core PUBLIC _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(telemetry_core PUBLIC ws2_32 mswsock advapi32 iphlpapi bcrypt crypt32)
endif()

# The executable is only built once a main.cpp exists next to this file.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    add_executable(telemetry_service main.cpp)
    target_link_libraries(telemetry_service PRIVATE telemetry_core)

    # MinGW: build its runtime (libwinpthread, libstdc++, libgcc) into the .exe, so it
    # runs outside CLion without needing those DLLs next to it.
    if(MINGW)
        target_link_options(telemetry_service PRIVATE -static)
    endif()
endif()
