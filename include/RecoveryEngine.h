#pragma once
#include "FAT32Analyzer.h"
#include "HashCalculator.h"
#include "AuditLogger.h"
#include <memory>

class RecoveryEngine {
public:
    RecoveryEngine(FAT32Analyzer& fs, DriveManager& drive,
                   std::shared_ptr<AuditLogger> logger)
        : fs_(fs), drive_(drive), logger_(std::move(logger)) {}

    bool RecoverOne(size_t index, const std::string& outDir);
    bool RecoverMany(const std::vector<size_t>& indices,
                     const std::string& outDir);
    bool RecoverAll(const std::string& outDir);

private:
    FAT32Analyzer& fs_;
    DriveManager&  drive_;
    std::shared_ptr<AuditLogger> logger_;

    std::string BuildOutputPath(const FileRecord& rec,
                                const std::string& outDir) const;
    bool ComputeSourceHash(const FileRecord& rec,
                           std::string& outMd5, std::string& outSha256);
};