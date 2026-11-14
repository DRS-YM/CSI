cmake_minimum_required(VERSION 3.20)
project(FAT32FileRecoveryTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source files are currently stored in the repository root.
add_executable(FileRecoveryTool
    main.cpp
    DriveManager.cpp
    FAT32Analyzer.cpp
    HashCalculator.cpp
    RecoveryEngine.cpp
    AuditLogger.cpp
    UI.cpp
)

target_include_directories(FileRecoveryTool PRIVATE ".")

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
