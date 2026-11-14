#include "FAT32Analyzer.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace { bool IsPowerOfTwo(uint32_t n) { return n && !(n & (n - 1)); } }

bool FAT32Analyzer::IsValidCluster(uint32_t c) const {
    return c >= 2 && static_cast<uint64_t>(c) < static_cast<uint64_t>(totalClusters) + 2;
}

bool FAT32Analyzer::LoadBootSector() {
    std::vector<uint8_t> sector(512);
    if (!drive.ReadBytes(0, sector.data(), 512) ||
        sector[510] != 0x55 || sector[511] != 0xAA) {
        std::cerr << "[!] Cannot read a valid FAT boot sector.\n";
        return false;
    }
    std::memcpy(&bs, sector.data(), sizeof(BootSector));
    if (bs.bytesPerSector < 512 || bs.bytesPerSector > 4096 ||
        !IsPowerOfTwo(bs.bytesPerSector) ||
        !IsPowerOfTwo(bs.sectorsPerCluster) || bs.sectorsPerCluster > 128 ||
        !bs.reservedSectorCount || !bs.numFATs || !bs.FATSize32 ||
        bs.rootEntryCount != 0 || bs.FATSize16 != 0 || bs.fsVersion != 0) {
        std::cerr << "[!] Invalid or unsupported FAT32 parameter block.\n";
        return false;
    }

    const uint64_t sectors = bs.totalSectors32 ? bs.totalSectors32 : bs.totalSectors16;
    const uint64_t fatSectors = bs.FATSize32;
    const uint64_t firstData = static_cast<uint64_t>(bs.reservedSectorCount) +
                               static_cast<uint64_t>(bs.numFATs) * fatSectors;
    if (sectors <= firstData) return false;

    clusterSize = static_cast<uint64_t>(bs.bytesPerSector) * bs.sectorsPerCluster;
    totalClusters = static_cast<uint32_t>((sectors - firstData) / bs.sectorsPerCluster);
    if (!totalClusters || !IsValidCluster(bs.rootCluster) ||
        clusterSize > std::numeric_limits<uint32_t>::max()) return false;

    fatOffset = static_cast<uint64_t>(bs.reservedSectorCount) * bs.bytesPerSector;
    dataOffset = firstData * bs.bytesPerSector;
    const uint64_t volumeBytes = sectors * bs.bytesPerSector;
    const uint64_t fatBytes = fatSectors * bs.bytesPerSector;
    if ((drive.GetDiskSize() && volumeBytes > drive.GetDiskSize()) ||
        fatBytes > std::numeric_limits<uint32_t>::max() ||
        fatBytes < (static_cast<uint64_t>(totalClusters) + 2) * 4 ||
        dataOffset > volumeBytes ||
        static_cast<uint64_t>(totalClusters) * clusterSize > volumeBytes - dataOffset) {
        std::cerr << "[!] FAT32 geometry exceeds device or supported bounds.\n";
        return false;
    }

    // Respect the selected FAT when mirroring is disabled.
    uint32_t activeFat = 0;
    if (bs.extFlags & 0x0080) {
        activeFat = bs.extFlags & 0x000F;
        if (activeFat >= bs.numFATs) return false;
    }
    fatOffset += static_cast<uint64_t>(activeFat) * fatBytes;
    return true;
}

bool FAT32Analyzer::LoadFATTable() {
    const uint64_t bytes = static_cast<uint64_t>(bs.FATSize32) * bs.bytesPerSector;
    if (!bytes || bytes > std::numeric_limits<uint32_t>::max()) return false;
    fatCache.resize(static_cast<size_t>(bytes));
    if (!drive.ReadBytes(fatOffset, fatCache.data(), static_cast<uint32_t>(bytes))) {
        fatCache.clear();
        return false;
    }
    return true;
}

uint32_t FAT32Analyzer::GetNextCluster(uint32_t cluster) {
    if (!IsValidCluster(cluster)) return FAT32_EOC;
    const uint64_t index = static_cast<uint64_t>(cluster) * 4;
    if (index + 4 > fatCache.size()) return FAT32_EOC;
    uint32_t value = 0;
    std::memcpy(&value, fatCache.data() + index, sizeof(value));
    return value & 0x0FFFFFFF;
}

uint64_t FAT32Analyzer::ClusterToOffset(uint32_t cluster) const {
    if (!IsValidCluster(cluster)) return 0;
    return dataOffset + static_cast<uint64_t>(cluster - 2) * clusterSize;
}

bool FAT32Analyzer::ReadCluster(uint32_t cluster, std::vector<uint8_t>& buffer) {
    if (!IsValidCluster(cluster)) return false;
    buffer.resize(static_cast<size_t>(clusterSize));
    const uint64_t offset = ClusterToOffset(cluster);
    return offset && drive.ReadBytes(offset, buffer.data(), static_cast<uint32_t>(clusterSize));
}

std::string FAT32Analyzer::ParseName(const FAT32DirEntry& e) const {
    std::string name, ext;
    for (int i = 0; i < 8 && e.name[i] != ' '; ++i) {
        const uint8_t c = static_cast<uint8_t>(e.name[i]);
        name += c == DELETED_MARKER ? '?' : static_cast<char>(c);
    }
    for (int i = 0; i < 3 && e.ext[i] != ' '; ++i) {
        const uint8_t c = static_cast<uint8_t>(e.ext[i]);
        ext += c == DELETED_MARKER ? '?' : static_cast<char>(c);
    }
    return ext.empty() ? name : name + "." + ext;
}

void FAT32Analyzer::ScanDirectory(uint32_t start, const std::string& parentPath) {
    (void)parentPath; // Full relative paths are not yet represented in FileRecord.
    if (!IsValidCluster(start) || directoryDepth >= 128 ||
        !scannedDirectoryClusters.insert(start).second) {
        if (directoryDepth >= 128)
            std::cerr << "[!] Maximum directory nesting depth reached.\\n";
        return;
    }
    ++directoryDepth;
    struct DepthGuard {
        uint32_t& depth;
        ~DepthGuard() { --depth; }
    } depthGuard{directoryDepth};
    uint32_t cluster = start;

    for (uint32_t visited = 0; visited < totalClusters; ++visited) {
        std::vector<uint8_t> data;
        if (!ReadCluster(cluster, data)) {
            std::cerr << "[!] Failed to read directory cluster " << cluster << ".\n";
            return;
        }
        const size_t count = data.size() / sizeof(FAT32DirEntry);
        for (size_t i = 0; i < count; ++i) {
            FAT32DirEntry e{};
            std::memcpy(&e, data.data() + i * sizeof(e), sizeof(e));
            const uint8_t first = static_cast<uint8_t>(e.name[0]);
            if (first == 0x00) return;
            if ((e.attributes & ATTR_LONG_NAME) == ATTR_LONG_NAME ||
                (e.attributes & ATTR_VOLUME_ID)) continue;
            if (e.name[0] == '.' && (e.name[1] == ' ' || e.name[1] == '.')) continue;

            const bool deleted = first == DELETED_MARKER;
            const bool directory = (e.attributes & ATTR_DIRECTORY) != 0;
            FileRecord r;
            r.name = ParseName(e);
            r.size = e.fileSize;
            r.startCluster = (static_cast<uint32_t>(e.firstClusterHigh) << 16) | e.firstClusterLow;
            r.parentCluster = cluster;
            r.isDeleted = deleted;
            r.isDirectory = directory;
            r.attributes = e.attributes;
            records.push_back(r);

            if (directory && !deleted && IsValidCluster(r.startCluster))
                ScanDirectory(r.startCluster, parentPath + r.name + "/");
        }
        const uint32_t next = GetNextCluster(cluster);
        if (next >= FAT32_EOC || !IsValidCluster(next)) return;
        cluster = next;
    }
    std::cerr << "[!] Directory traversal limit reached (possible FAT loop).\n";
}

bool FAT32Analyzer::Parse() {
    records.clear();
    fatCache.clear();
    scannedDirectoryClusters.clear();
    directoryDepth = 0;
    if (!LoadBootSector() || !LoadFATTable()) return false;

    std::cout << "[+] FAT32 metadata validated.\n"
              << "    - Bytes per Sector : " << bs.bytesPerSector << "\n"
              << "    - Cluster Size     : " << clusterSize << " bytes\n"
              << "    - Root Cluster     : " << bs.rootCluster << "\n"
              << "    - Total Clusters   : " << totalClusters << "\n"
              << "    - Volume Label     : " << std::string(bs.volumeLabel, 11) << "\n";
    ScanDirectory(bs.rootCluster, "/");
    size_t deleted = 0;
    for (const auto& r : records) if (r.isDeleted && !r.isDirectory) ++deleted;
    std::cout << "[+] Scan complete: " << records.size()
              << " entries, " << deleted << " deleted files.\n";
    return true;
}

bool FAT32Analyzer::RecoverFile(const FileRecord& rec, const std::string& outputPath) {
    if (rec.isDirectory || rec.size == 0 || !IsValidCluster(rec.startCluster) || !clusterSize)
        return false;

    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    uint64_t remaining = rec.size;
    uint32_t cluster = rec.startCluster, visited = 0;
    std::unordered_set<uint32_t> seen;
    std::vector<uint8_t> buffer(static_cast<size_t>(clusterSize));
    bool ok = true;

    while (remaining) {
        if (!IsValidCluster(cluster) || !seen.insert(cluster).second || visited++ >= totalClusters) {
            ok = false;
            break;
        }
        if (!drive.ReadBytes(ClusterToOffset(cluster), buffer.data(), static_cast<uint32_t>(clusterSize))) {
            ok = false;
            break;
        }
        const uint64_t amount = (std::min)(remaining, clusterSize);
        out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(amount));
        if (!out) { ok = false; break; }
        remaining -= amount;
        if (!remaining) break;
        const uint32_t next = GetNextCluster(cluster);
        if (!IsValidCluster(next)) { ok = false; break; }
        cluster = next;
    }
    out.close();
    if (!out) ok = false;
    if (!ok || remaining) {
        std::error_code ec;
        std::filesystem::remove(outputPath, ec);
        std::cerr << "[!] Incomplete recovery; partial output removed. FAT chain may be missing or damaged.\n";
        return false;
    }
    return true;
}
