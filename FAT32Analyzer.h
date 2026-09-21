#pragma once
#include "DriveManager.h"
#include "FileRecord.h"
#include <vector>
#include <cstdint>
#include <string>
#include <chrono>

#pragma pack(push, 1)

struct BootSector {
    uint8_t  jumpBoot[3];
    char     oemName[8];
    uint16_t bytesPerSector;
    uint8_t  sectorsPerCluster;
    uint16_t reservedSectorCount;
    uint8_t  numFATs;
    uint16_t rootEntryCount;
    uint16_t totalSectors16;
    uint8_t  media;
    uint16_t FATSize16;
    uint16_t sectorsPerTrack;
    uint16_t numberOfHeads;
    uint32_t hiddenSectors;
    uint32_t totalSectors32;
    // --- FAT32 Extended ---
    uint32_t FATSize32;
    uint16_t extFlags;
    uint16_t fsVersion;
    uint32_t rootCluster;
    uint16_t fsInfoSector;
    uint16_t backupBootSector;
    uint8_t  reserved[12];
    uint8_t  driveNumber;
    uint8_t  reserved1;
    uint8_t  bootSignature;
    uint32_t volumeID;
    char     volumeLabel[11];
    char     fsType[8];
};

struct FAT32DirEntry {
    char     name[8];
    char     ext[3];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creationTimeTenth;
    uint16_t creationTime;
    uint16_t creationDate;
    uint16_t lastAccessDate;
    uint16_t firstClusterHigh;
    uint16_t writeTime;
    uint16_t writeDate;
    uint16_t firstClusterLow;
    uint32_t fileSize;
};

#pragma pack(pop)

class FAT32Analyzer {
public:
    explicit FAT32Analyzer(DriveManager& d) : drive(d) {}

    bool Parse();

    uint64_t GetClusterSize() const { return clusterSize; }
    uint64_t ClusterToOffset(uint32_t cluster) const;
    uint32_t GetNextCluster(uint32_t cluster);

    const std::vector<FileRecord>& GetRecords() const { return records; }

    bool RecoverFile(const FileRecord& rec, const std::string& outputPath);

private:
    DriveManager& drive;
    BootSector    bs{};
    uint64_t      fatOffset   = 0;
    uint64_t      dataOffset  = 0;
    uint64_t      clusterSize = 0;
    uint32_t      totalClusters = 0;

    std::vector<FileRecord> records;
    std::vector<uint8_t>    fatCache;

    bool LoadBootSector();
    bool LoadFATTable();
    void ScanDirectory(uint32_t cluster, const std::string& parentPath);
    std::vector<uint8_t> ReadCluster(uint32_t cluster);
    std::string ParseName(const FAT32DirEntry& e) const;
};

static constexpr uint8_t  ATTR_LONG_NAME = 0x0F;
static constexpr uint8_t  ATTR_DIRECTORY = 0x10;
static constexpr uint8_t  ATTR_VOLUME_ID = 0x08;
static constexpr uint8_t  DELETED_MARKER = 0xE5;
static constexpr uint32_t FAT32_EOC      = 0x0FFFFFF8;