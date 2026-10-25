cmake_minimum_required(VERSION 3.21)

# Let CLion / command-line builds find vcpkg packages (needs the VCPKG_ROOT
# environment variable). Must be set before project().
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()

# vcpkg: build compiled libraries (libpqxx, libpq) for MinGW, statically, to match the
# -static .exe below. Without this vcpkg builds MSVC libraries that MinGW can't link.
# Must be set before project(). Remove these lines if you ever switch to MSVC.
if(CMAKE_HOST_WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-mingw-static" CACHE STRING "vcpkg target triplet")
    set(VCPKG_HOST_TRIPLET   "x64-mingw-static" CACHE STRING "vcpkg host triplet")
endif()

project(Wireless_Telemetry CXX)

set(CMAKE_CXX_STANDARD 20)   # libpqxx 8 needs C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(asio CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)

# MinGW + static libpq: libpgcommon uses functions from libpgport, but vcpkg puts libpgport
# BEFORE libpgcommon on the link line, and the MinGW linker only looks forward. Repeat both
# at the very end of the link line so those functions are found.
if(MINGW AND VCPKG_TARGET_TRIPLET MATCHES "static")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(PG_LIB_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib")
    else()
        set(PG_LIB_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib")
    endif()
    set(CMAKE_CXX_STANDARD_LIBRARIES
        "${PG_LIB_DIR}/libpgcommon.a ${PG_LIB_DIR}/libpgport.a -lws2_32 -lsecur32 ${CMAKE_CXX_STANDARD_LIBRARIES}")
endif()

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
        DesktopReciverService/TelemtryRepsoitory.cpp
        DesktopReciverService/TelemtryRepsoitory.h
        DesktopReciverService/GrafanaLiveStream.cpp
        DesktopReciverService/GrafanaLiveStream.h
)
target_include_directories(telemetry_core PUBLIC DesktopReciverService)
target_link_libraries(telemetry_core PUBLIC asio::asio libpqxx::pqxx cpr::cpr)
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
