#include "DriveManager.h"
#include "FAT32Analyzer.h"
#include "RecoveryEngine.h"
#include "AuditLogger.h"
#include "UI.h"
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string.h>

namespace {
bool IsSameVolumeOrUnknown(const std::string& sourceDrive,
                           const std::string& destinationPath) {
    char absolutePath[MAX_PATH]{};
    const DWORD length = GetFullPathNameA(
        destinationPath.c_str(), MAX_PATH, absolutePath, nullptr);
    if (length == 0 || length >= MAX_PATH) return true;

    std::filesystem::path existingPath(absolutePath);
    std::error_code error;
    while (!std::filesystem::exists(existingPath, error)) {
        error.clear();
        const auto parent = existingPath.parent_path();
        if (parent.empty() || parent == existingPath) return true;
        existingPath = parent;
    }

    char destinationRoot[MAX_PATH]{};
    const std::string probe = existingPath.string();
    if (!GetVolumePathNameA(probe.c_str(), destinationRoot, MAX_PATH))
        return true;

    const std::string sourceRoot = sourceDrive + "\\";
    char sourceVolume[MAX_PATH]{}, destinationVolume[MAX_PATH]{};
    if (!GetVolumeNameForVolumeMountPointA(
            sourceRoot.c_str(), sourceVolume, MAX_PATH) ||
        !GetVolumeNameForVolumeMountPointA(
            destinationRoot, destinationVolume, MAX_PATH))
        return true;

    return _stricmp(sourceVolume, destinationVolume) == 0;
}
}

int main() {
    try {
        std::cout << "===== FAT32 Digital Forensics & File Recovery =====\n\n";
        std::string caseId, examiner, driveLetter, auditDir;
        std::cout << "Case ID        : ";
        if (!std::getline(std::cin, caseId) || caseId.empty()) {
            std::cerr << "[!] A case ID is required.\n";
            return 1;
        }
        std::cout << "Examiner Name  : ";
        if (!std::getline(std::cin, examiner)) return 1;
        std::cout << "Source Drive   : ";
        if (!std::getline(std::cin, driveLetter) || driveLetter.empty()) {
            std::cerr << "[!] A drive letter such as E: is required.\n";
            return 1;
        }

        DriveManager drive;
        if (!drive.Open(driveLetter)) return 1;

        std::cout << "Audit report directory (different volume, e.g. D:\\RecoveryReports): ";
        if (!std::getline(std::cin >> std::ws, auditDir) || auditDir.empty()) {
            std::cerr << "[!] An audit report directory is required.\n";
            return 1;
        }
        if (IsSameVolumeOrUnknown(drive.GetDriveLetter(), auditDir)) {
            std::cerr << "[!] Choose a verifiable report directory on a different volume from the source.\n";
            return 1;
        }

        FAT32Analyzer analyzer(drive);
        if (!analyzer.Parse()) {
            std::cerr << "[!] FAT32 analysis could not be completed.\n";
            return 1;
        }

        auto logger = std::make_shared<AuditLogger>(
            caseId, examiner, drive.GetDriveLetter(), auditDir);
        RecoveryEngine engine(analyzer, drive, logger);
        UI ui(analyzer, engine);
        ui.ShowMenu();

        const bool textOk = logger->WriteTextReport();
        const bool csvOk = logger->WriteCsvReport();
        const bool jsonOk = logger->WriteJsonReport();
        logger->PrintSummary();

        if (!textOk || !csvOk || !jsonOk) {
            std::cerr << "[!] One or more audit reports could not be written.\n";
            return 2;
        }
        std::cout << "\n[+] Reports saved in: " << auditDir << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[!] Fatal error: " << error.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "[!] An unknown fatal error occurred.\n";
        return 1;
    }
}
