cmake_minimum_required(VERSION 3.15)
project(FileRecoveryTool CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(FileRecoveryTool
    src/main.cpp
    src/DriveManager.cpp
    src/HashCalculator.cpp
    src/AuditLogger.cpp
    src/FAT32Analyzer.cpp
    src/RecoveryEngine.cpp
    src/UI.cpp
)

target_include_directories(FileRecoveryTool PRIVATE include)
target_link_libraries(FileRecoveryTool PRIVATE bcrypt)


# إجبار المترجم على دمج مكتبات C++ الأساسية داخل البرنامج
target_link_options(FileRecoveryTool PRIVATE "-static" "-static-libgcc" "-static-libstdc++")