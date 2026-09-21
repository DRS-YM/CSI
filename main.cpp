#include "DriveManager.h"
#include "FAT32Analyzer.h"
#include "RecoveryEngine.h"
#include "AuditLogger.h"
#include "UI.h"
#include <iostream>
#include <memory>

int main() {
    std::cout << "===== Digital Forensics: File Recovery Tool =====\n\n";

    std::string caseId, examiner, driveLetter;
    std::cout << "Case ID        : "; std::getline(std::cin, caseId);
    std::cout << "Examiner Name  : "; std::getline(std::cin, examiner);
    std::cout << "Drive Letter   : "; std::getline(std::cin, driveLetter);

    DriveManager drive;
    if (!drive.Open(driveLetter)) {
        std::cerr << "[!] Make sure you are running as Administrator.\n";
        return 1;
    }

    FAT32Analyzer analyzer(drive);
    if (!analyzer.Parse()) {
        std::cerr << "[!] Parsing failed.\n";
        return 1;
    }

    std::string auditDir = "RecoveryReports_" + caseId;
    auto logger = std::make_shared<AuditLogger>(caseId, examiner,
                                                driveLetter, auditDir);

    RecoveryEngine engine(analyzer, drive, logger);

    UI ui(analyzer, engine);
    ui.ShowMenu();

    logger->WriteTextReport();
    logger->WriteCsvReport();
    logger->WriteJsonReport();
    logger->PrintSummary();

    std::cout << "\n[+] Reports saved in: " << auditDir << "\n";
    return 0;
}