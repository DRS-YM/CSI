cmake_minimum_required(VERSION 3.20)
project(FAT32FileRecoveryTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(FileRecoveryTool
    src/main.cpp
    src/DriveManager.cpp
    src/FAT32Analyzer.cpp
    src/HashCalculator.cpp
    src/RecoveryEngine.cpp
    src/AuditLogger.cpp
    src/UI.cpp
)

target_include_directories(FileRecoveryTool PRIVATE include)
# Prevent the Win32 min/max macros from breaking standard-library calls.
target_compile_definitions(FileRecoveryTool PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

if(WIN32)
    target_link_libraries(FileRecoveryTool PRIVATE bcrypt)
else()
    message(FATAL_ERROR "FileRecoveryTool uses the Windows API and must be built on Windows.")
endif()

if(MSVC)
    target_compile_options(FileRecoveryTool PRIVATE /W4 /permissive-)
else()
    target_compile_options(FileRecoveryTool PRIVATE -Wall -Wextra -Wpedantic)
endif()
