#include "RecoveryEngine.h"
#include <filesystem>
#include <iostream>
#include <chrono>

namespace fs = std::filesystem;

std::string RecoveryEngine::BuildOutputPath(const FileRecord& rec,
                                            const std::string& outDir) const {
    fs::create_directories(outDir);
    std::string safeName = rec.FullName();
    for (auto& c : safeName)
        if (c == '?' || c == '/' || c == '\\' || c == ':') c = '_';

    fs::path p = fs::path(outDir) / safeName;
    int counter = 1;
    while (fs::exists(p))
        p = fs::path(outDir) / (safeName + "_" + std::to_string(counter++));
    return p.string();
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

    uint64_t clusterSize = fs_.GetClusterSize();
    uint32_t cluster     = rec.startCluster;
    uint64_t remaining   = rec.size;

    HashCalculator::StreamHasher md5(HashAlgorithm::MD5);
    HashCalculator::StreamHasher sha(HashAlgorithm::SHA256);
    std::vector<uint8_t> buf(clusterSize);

    while (remaining > 0 && cluster >= 2 && cluster < FAT32_EOC) {
        uint64_t off = fs_.ClusterToOffset(cluster);
        if (!drive_.ReadBytes(off, buf.data(),
                              static_cast<uint32_t>(clusterSize)))
            return false;

        uint64_t toRead = (std::min)(remaining, clusterSize);
        md5.Update(buf.data(), static_cast<size_t>(toRead));
        sha.Update(buf.data(), static_cast<size_t>(toRead));
        remaining -= toRead;
        cluster = fs_.GetNextCluster(cluster);
    }

    auto r1 = md5.Finalize();
    auto r2 = sha.Finalize();
    if (!r1.success || !r2.success) return false;

    outMd5    = r1.hexDigest;
    outSha256 = r2.hexDigest;
    return true;
}

bool RecoveryEngine::RecoverOne(size_t index, const std::string& outDir) {
    const auto& records = fs_.GetRecords();
    if (index >= records.size()) return false;
    const auto& rec = records[index];
    if (rec.isDirectory) return false;

    std::string outPath = BuildOutputPath(rec, outDir);

    std::cout << "\n[*] Recovering: " << rec.FullName()
              << " (" << rec.ReadableSize() << ")\n";

    RecoveryLogEntry entry;
    entry.fileName      = rec.FullName();
    entry.fileSize      = rec.size;
    entry.startCluster  = rec.startCluster;
    entry.wasDeleted    = rec.isDeleted;
    entry.recoveredPath = outPath;
    entry.recoveryTime  = std::chrono::system_clock::now();

    auto t0 = std::chrono::steady_clock::now();

    std::cout << "    [*] Hashing source (raw disk)...\n";
    std::string srcMd5, srcSha;
    if (!ComputeSourceHash(rec, srcMd5, srcSha)) {
        std::cerr << "    [!] Failed to hash source.\n";
        return false;
    }
    entry.sourceMd5    = srcMd5;
    entry.sourceSha256 = srcSha;

    bool ok = fs_.RecoverFile(rec, outPath);
    if (!ok) {
        std::cerr << "    [!] Recovery failed.\n";
        entry.integrityOk = false;
        if (logger_) logger_->AddEntry(entry);
        return false;
    }

    std::cout << "    [*] Hashing recovered file...\n";
    auto dMd5 = HashCalculator::HashFile(outPath, HashAlgorithm::MD5);
    auto dSha = HashCalculator::HashFile(outPath, HashAlgorithm::SHA256);
    entry.destMd5    = dMd5.hexDigest;
    entry.destSha256 = dSha.hexDigest;

    entry.integrityOk = (entry.sourceSha256 == entry.destSha256) &&
                        (entry.sourceMd5    == entry.destMd5);

    auto t1 = std::chrono::steady_clock::now();
    entry.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>
                      (t1 - t0).count();

    std::cout << "    Source SHA-256: " << entry.sourceSha256 << "\n"
              << "    Dest   SHA-256: " << entry.destSha256 << "\n"
              << "    Integrity: "
              << (entry.integrityOk ? "VERIFIED\n" : "MISMATCH\n");

    if (logger_) logger_->AddEntry(entry);
    return entry.integrityOk;
}

bool RecoveryEngine::RecoverMany(const std::vector<size_t>& indices,
                                 const std::string& outDir) {
    bool allOk = true;
    for (auto i : indices) allOk &= RecoverOne(i, outDir);
    return allOk;
}

bool RecoveryEngine::RecoverAll(const std::string& outDir) {
    const auto& records = fs_.GetRecords();
    bool allOk = true;
    for (size_t i = 0; i < records.size(); ++i) {
        if (records[i].isDirectory) continue;
        if (!records[i].isDeleted)  continue;
        allOk &= RecoverOne(i, outDir);
    }
    return allOk;
}