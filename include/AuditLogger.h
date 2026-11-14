#pragma once
#include "FileRecord.h"
#include "HashCalculator.h"
#include <string>
#include <vector>
#include <chrono>

struct RecoveryLogEntry {
    std::string caseId;
    std::string examinerName;
    std::string driveLetter;

    std::string fileName;
    uint64_t    fileSize      = 0;
    uint32_t    startCluster  = 0;
    bool        wasDeleted    = false;

    std::string sourceMd5, sourceSha256;
    std::string destMd5,   destSha256;
    bool        integrityOk = false;

    std::string recoveredPath;
    std::chrono::system_clock::time_point recoveryTime;
    uint64_t    elapsedMs = 0;
};

class AuditLogger {
public:
    AuditLogger(const std::string& caseId,
                const std::string& examinerName,
                const std::string& driveLetter,
                const std::string& outputDir);

    void AddEntry(const RecoveryLogEntry& entry);

    bool WriteTextReport();
    bool WriteCsvReport();
    bool WriteJsonReport();
    void PrintSummary() const;

private:
    std::string caseId_, examinerName_, driveLetter_, outputDir_, timestamp_;
    std::vector<RecoveryLogEntry> entries_;

    std::string TimestampString(std::chrono::system_clock::time_point tp) const;
    std::string SafeFilePrefix() const;
    static std::string EscapeJson(const std::string& s);
    static std::string EscapeCsv(const std::string& s);
};