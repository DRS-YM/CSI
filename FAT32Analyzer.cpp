#include "FAT32Analyzer.h"
#include <iostream>
#include <cstring>
#include <fstream>
#include <algorithm>

bool FAT32Analyzer::LoadBootSector() {
    std::vector<uint8_t> buffer(512);
    if (!drive.ReadBytes(0, buffer.data(), 512)) return false;

    memcpy(&bs, buffer.data(), sizeof(BootSector));

    if (bs.bytesPerSector == 0 || bs.sectorsPerCluster == 0) return false;
    if (bs.FATSize32 == 0) {
        std::cerr << "[!] This is not a standard FAT32 drive.\n";
        return false;
    }

    clusterSize   = bs.bytesPerSector * bs.sectorsPerCluster;
    fatOffset     = static_cast<uint64_t>(bs.reservedSectorCount)
                    * bs.bytesPerSector;
    dataOffset    = fatOffset
                    + static_cast<uint64_t>(bs.numFATs) * bs.FATSize32
                      * bs.bytesPerSector;
    totalClusters = (bs.totalSectors32 -
                     (bs.reservedSectorCount + bs.numFATs * bs.FATSize32))
                    / bs.sectorsPerCluster;

    return true;
}

bool FAT32Analyzer::LoadFATTable() {
    uint64_t fatBytes = static_cast<uint64_t>(bs.FATSize32)
                        * bs.bytesPerSector;
    fatCache.resize(fatBytes);
    return drive.ReadBytes(fatOffset, fatCache.data(),
                           static_cast<uint32_t>(fatBytes));
}

uint32_t FAT32Analyzer::GetNextCluster(uint32_t cluster) {
    uint64_t idx = static_cast<uint64_t>(cluster) * 4;
    if (idx + 4 > fatCache.size()) return FAT32_EOC;
    uint32_t val;
    memcpy(&val, fatCache.data() + idx, 4);
    return val & 0x0FFFFFFF;
}

uint64_t FAT32Analyzer::ClusterToOffset(uint32_t cluster) const {
    if (cluster < 2) return 0;
    return dataOffset + static_cast<uint64_t>(cluster - 2) * clusterSize;
}

std::vector<uint8_t> FAT32Analyzer::ReadCluster(uint32_t cluster) {
    std::vector<uint8_t> buf(clusterSize);
    drive.ReadBytes(ClusterToOffset(cluster), buf.data(),
                    static_cast<uint32_t>(clusterSize));
    return buf;
}

std::string FAT32Analyzer::ParseName(const FAT32DirEntry& e) const {
    std::string name;
    for (int i = 0; i < 8 && e.name[i] != ' '; ++i) {
        uint8_t c = static_cast<uint8_t>(e.name[i]);
        if (c == DELETED_MARKER) name += '?';
        else name += static_cast<char>(c);
    }
    std::string ext;
    for (int i = 0; i < 3 && e.ext[i] != ' '; ++i) {
        uint8_t c = static_cast<uint8_t>(e.ext[i]);
        if (c == DELETED_MARKER) ext += '?';
        else ext += static_cast<char>(c);
    }
    if (!ext.empty()) name += "." + ext;
    return name;
}

void FAT32Analyzer::ScanDirectory(uint32_t startCluster,
                                  const std::string& parentPath) {
    if (startCluster < 2) return;

    uint32_t cluster = startCluster;

    while (cluster >= 2 && cluster < FAT32_EOC) {
        auto data = ReadCluster(cluster);
        auto* entries = reinterpret_cast<FAT32DirEntry*>(data.data());
        size_t entryCount = clusterSize / sizeof(FAT32DirEntry);

        for (size_t i = 0; i < entryCount; ++i) {
            const auto& e = entries[i];
            uint8_t firstByte = static_cast<uint8_t>(e.name[0]);

            if (firstByte == 0x00) return;
            if ((e.attributes & ATTR_LONG_NAME) == ATTR_LONG_NAME) continue;
            if (e.attributes & ATTR_VOLUME_ID) continue;

            bool deleted = (firstByte == DELETED_MARKER);
            bool isDir   = (e.attributes & ATTR_DIRECTORY) != 0;

            if (e.name[0] == '.' && (e.name[1] == ' ' || e.name[1] == '.'))
                continue;

            FileRecord rec;
            rec.name         = ParseName(e);
            rec.size         = e.fileSize;
            rec.startCluster = (static_cast<uint32_t>(e.firstClusterHigh) << 16)
                               | e.firstClusterLow;
            rec.parentCluster = cluster;
            rec.isDeleted    = deleted;
            rec.isDirectory  = isDir;
            rec.attributes   = e.attributes;

            records.push_back(rec);

            if (isDir && !deleted && rec.startCluster >= 2) {
                ScanDirectory(rec.startCluster,
                              parentPath + rec.name + "/");
            }
        }
        cluster = GetNextCluster(cluster);
    }
}

bool FAT32Analyzer::Parse() {
    records.clear();
    fatCache.clear();

    if (!LoadBootSector()) return false;
    if (!LoadFATTable())   return false;

    std::cout << "[+] FAT32 Parsed Successfully!\n"
              << "    - Bytes per Sector : " << bs.bytesPerSector << "\n"
              << "    - Cluster Size     : " << clusterSize << " bytes\n"
              << "    - Root Cluster     : " << bs.rootCluster << "\n"
              << "    - Total Clusters   : " << totalClusters << "\n"
              << "    - Volume Label     : "
              << std::string(bs.volumeLabel, 11) << "\n";

    ScanDirectory(bs.rootCluster, "/");

    size_t deletedCount = 0;
    for (const auto& r : records) if (r.isDeleted) ++deletedCount;

    std::cout << "[+] Scan complete:\n"
              << "    - Total entries    : " << records.size() << "\n"
              << "    - Deleted files    : " << deletedCount << "\n";
    return true;
}

bool FAT32Analyzer::RecoverFile(const FileRecord& rec,
                                const std::string& outputPath) {
    if (rec.isDirectory) return false;
    if (rec.size == 0)   return false;
    if (rec.startCluster < 2) return false;

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        std::cerr << "[!] Cannot create output: " << outputPath << "\n";
        return false;
    }

    uint64_t remaining = rec.size;
    uint32_t cluster   = rec.startCluster;
    std::vector<uint8_t> buf(clusterSize);

    while (remaining > 0 && cluster >= 2 && cluster < FAT32_EOC) {
        uint64_t off = ClusterToOffset(cluster);
        if (!drive.ReadBytes(off, buf.data(),
                             static_cast<uint32_t>(clusterSize))) break;

        uint64_t toWrite = (std::min)(remaining, clusterSize);
        out.write(reinterpret_cast<const char*>(buf.data()), toWrite);
        remaining -= toWrite;
        cluster = GetNextCluster(cluster);
    }

    out.close();
    return remaining == 0;
}