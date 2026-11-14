#include "DriveManager.h"
#include "FAT32Analyzer.h"
#include "RecoveryEngine.h"
#include "AuditLogger.h"
#include "UI.h"
#include <cctype>
#include <iostream>
#include <memory>
#include <string>

namespace {
std::string SafeDirectoryToken(std::string value) {
    for (char& ch : value) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 32 || ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
            ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*')
            ch = '_';
    }
    while (!value.empty() && (value.back() == '.' || value.back() == ' '))
        value.pop_back();
    return value.empty() ? "case" : value;
}
}

int main() {
    try {
        std::cout << "===== FAT32 Digital Forensics & File Recovery =====\n\n";
        std::string caseId, examiner, driveLetter;
        std::cout << "Case ID        : ";
        if (!std::getline(std::cin, caseId) || caseId.empty()) {
            std::cerr << "[!] A case ID is required.\n";
            return 1;
        }
        std::cout << "Examiner Name  : ";
        if (!std::getline(std::cin, examiner)) return 1;
        std::cout << "Drive Letter   : ";
        if (!std::getline(std::cin, driveLetter) || driveLetter.empty()) {
            std::cerr << "[!] A drive letter such as E: is required.\n";
            return 1;
        }

        DriveManager drive;
        if (!drive.Open(driveLetter)) return 1;

        FAT32Analyzer analyzer(drive);
        if (!analyzer.Parse()) {
            std::cerr << "[!] FAT32 analysis could not be completed.\n";
            return 1;
        }

        const std::string auditDir =
            "RecoveryReports_" + SafeDirectoryToken(caseId);
        auto logger = std::make_shared<AuditLogger>(
            caseId, examiner, driveLetter, auditDir);
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
