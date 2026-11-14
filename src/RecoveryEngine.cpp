#include "RecoveryEngine.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <unordered_set>

namespace fs = std::filesystem;

std::string RecoveryEngine::BuildOutputPath(const FileRecord& rec,
                                            const std::string& outDir) const {
    if (outDir.empty()) return {};
    std::error_code error;
    fs::create_directories(outDir, error);
    if (error) return {};

    std::string safeName = rec.FullName();
    for (char& ch : safeName) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 32 || ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
            ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*')
            ch = '_';
    }
    while (!safeName.empty() &&
           (safeName.back() == '.' || safeName.back() == ' '))
        safeName.pop_back();
    if (safeName.empty()) safeName = "recovered_file";

    fs::path candidate = fs::path(outDir) / safeName;
    const fs::path original = candidate;
    unsigned int suffix = 1;
    while (fs::exists(candidate, error) && !error) {
        candidate = original.parent_path() /
            (original.stem().string() + "_" + std::to_string(suffix++) +
             original.extension().string());
    }
    if (error) return {};
    return candidate.string();
}

bool RecoveryEngine::ComputeSourceHash(const FileRecord& rec,
                                       std::string& outMd5,
                                       std::string& outSha256) {
    if (rec.size == 0) {
        outMd5 = "d41d8cd98f00b204e9800998ecf8427e";
        outSha256 = "e3b0c44298fc1c149afbf4c8996fb924"
                    "27ae41e4649b934ca495991b7852b855";
        return true;
    }
    if (fs_.GetClusterSize() == 0) return false;

    const uint64_t clusterSize = fs_.GetClusterSize();
    uint64_t remaining = rec.size;
    uint32_t cluster = rec.startCluster;
    std::unordered_set<uint32_t> seen;
    HashCalculator::StreamHasher md5(HashAlgorithm::MD5);
    HashCalculator::StreamHasher sha(HashAlgorithm::SHA256);
    std::vector<uint8_t> buffer(static_cast<size_t>(clusterSize));

    while (remaining > 0) {
        const uint64_t offset = fs_.ClusterToOffset(cluster);
        if (offset == 0 || !seen.insert(cluster).second ||
            !drive_.ReadBytes(offset, buffer.data(),
                              static_cast<uint32_t>(clusterSize))) return false;

        const uint64_t amount = (std::min)(remaining, clusterSize);
        if (!md5.Update(buffer.data(), static_cast<size_t>(amount)) ||
            !sha.Update(buffer.data(), static_cast<size_t>(amount))) return false;
        remaining -= amount;
        if (remaining == 0) break;

        const uint32_t next = fs_.GetNextCluster(cluster);
        if (next < 2 || next >= FAT32_EOC) return false;
        cluster = next;
    }

    const HashResult md5Result = md5.Finalize();
    const HashResult shaResult = sha.Finalize();
    if (!md5Result.success || !shaResult.success ||
        md5Result.bytesProcessed != rec.size ||
        shaResult.bytesProcessed != rec.size) return false;

    outMd5 = md5Result.hexDigest;
    outSha256 = shaResult.hexDigest;
    return true;
}

bool RecoveryEngine::RecoverOne(size_t index, const std::string& outDir) {
    const auto& records = fs_.GetRecords();
    if (index >= records.size()) return false;
    const FileRecord& rec = records[index];
    if (rec.isDirectory) return false;

    const std::string outPath = BuildOutputPath(rec, outDir);
    if (outPath.empty()) {
        std::cerr << "[!] Cannot create or access the recovery output directory.\n";
        return false;
    }

    std::cout << "\n[*] Recovering: " << rec.FullName()
              << " (" << rec.ReadableSize() << ")\n";
    RecoveryLogEntry entry;
    entry.fileName = rec.FullName();
    entry.fileSize = rec.size;
    entry.startCluster = rec.startCluster;
    entry.wasDeleted = rec.isDeleted;
    entry.recoveredPath = outPath;
    entry.recoveryTime = std::chrono::system_clock::now();
    const auto started = std::chrono::steady_clock::now();

    const auto finish = [&]() {
        entry.elapsedMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count());
        if (logger_) logger_->AddEntry(entry);
    };

    std::cout << "    [*] Hashing source bytes...\n";
    if (!ComputeSourceHash(rec, entry.sourceMd5, entry.sourceSha256)) {
        std::cerr << "    [!] Source hash failed; source chain may be unavailable.\n";
        finish();
        return false;
    }

    if (!fs_.RecoverFile(rec, outPath)) {
        std::cerr << "    [!] Recovery failed.\n";
        finish();
        return false;
    }

    std::cout << "    [*] Verifying recovered file...\n";
    const HashResult destinationMd5 =
        HashCalculator::HashFile(outPath, HashAlgorithm::MD5);
    const HashResult destinationSha =
        HashCalculator::HashFile(outPath, HashAlgorithm::SHA256);
    if (!destinationMd5.success || !destinationSha.success ||
        destinationMd5.bytesProcessed != rec.size ||
        destinationSha.bytesProcessed != rec.size) {
        std::error_code error;
        fs::remove(outPath, error);
        std::cerr << "    [!] Could not verify output; incomplete file removed.\n";
        finish();
        return false;
    }

    entry.destMd5 = destinationMd5.hexDigest;
    entry.destSha256 = destinationSha.hexDigest;
    entry.integrityOk = entry.sourceSha256 == entry.destSha256 &&
                        entry.sourceMd5 == entry.destMd5;
    finish();

    std::cout << "    Source SHA-256: " << entry.sourceSha256 << "\n"
              << "    Dest   SHA-256: " << entry.destSha256 << "\n"
              << "    Integrity: "
              << (entry.integrityOk ? "VERIFIED\n" : "MISMATCH\n");
    return entry.integrityOk;
}

bool RecoveryEngine::RecoverMany(const std::vector<size_t>& indices,
                                 const std::string& outDir) {
    bool allOk = true;
    for (const size_t index : indices) {
        if (!RecoverOne(index, outDir)) allOk = false;
    }
    return allOk;
}

bool RecoveryEngine::RecoverAll(const std::string& outDir) {
    const auto& records = fs_.GetRecords();
    bool allOk = true;
    for (size_t i = 0; i < records.size(); ++i) {
        if (records[i].isDirectory || !records[i].isDeleted) continue;
        if (!RecoverOne(i, outDir)) allOk = false;
    }
    return allOk;
}
