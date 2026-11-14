#pragma once
#include <windows.h>
#include <string>
#include <cstdint>

class DriveManager {
public:
    DriveManager();
    ~DriveManager();

    bool Open(const std::string& driveLetter);
    void Close();

    bool ReadBytes(uint64_t offset, uint8_t* buffer, uint32_t size);
    bool IsOpen() const { return hDrive != INVALID_HANDLE_VALUE; }

    uint32_t GetBytesPerSector() const { return bytesPerSector_; }
    uint64_t GetDiskSize()       const { return diskSize_; }

private:
    HANDLE   hDrive         = INVALID_HANDLE_VALUE;
    uint32_t bytesPerSector_ = 512;
    uint64_t diskSize_       = 0;
};