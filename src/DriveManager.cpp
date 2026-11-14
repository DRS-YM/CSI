#include "DriveManager.h"
#include <windows.h>
#include <winioctl.h>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>

DriveManager::DriveManager() = default;
DriveManager::~DriveManager() { Close(); }

bool DriveManager::Open(const std::string& requestedLetter) {
    Close();

    std::string letter = requestedLetter;
    letter.erase(std::remove_if(letter.begin(), letter.end(),
        [](unsigned char ch) { return std::isspace(ch) != 0; }), letter.end());
    if (letter.size() == 1) letter += ':';
    if (letter.size() != 2 || letter[1] != ':' ||
        !std::isalpha(static_cast<unsigned char>(letter[0]))) {
        std::cerr << "[!] Enter a drive letter such as E: (not a folder path).\n";
        return false;
    }
    letter[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(letter[0])));
    const std::string devicePath = "\\\\.\\" + letter;

    hDrive = CreateFileA(devicePath.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDrive == INVALID_HANDLE_VALUE) {
        std::cerr << "[!] Cannot open " << letter
                  << ". Windows error: " << GetLastError() << "\n";
        return false;
    }

    GET_LENGTH_INFORMATION lengthInfo{};
    DWORD returned = 0;
    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0,
                        &lengthInfo, sizeof(lengthInfo), &returned, nullptr) &&
        lengthInfo.Length.QuadPart > 0) {
        diskSize_ = static_cast<uint64_t>(lengthInfo.Length.QuadPart);
    } else {
        // Geometry is a fallback; it may be approximate on some devices.
        DISK_GEOMETRY geometry{};
        if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY, nullptr, 0,
                            &geometry, sizeof(geometry), &returned, nullptr)) {
            bytesPerSector_ = geometry.BytesPerSector;
            diskSize_ = static_cast<uint64_t>(geometry.Cylinders.QuadPart) *
                geometry.TracksPerCylinder * geometry.SectorsPerTrack *
                geometry.BytesPerSector;
        }
    }

    std::cout << "[+] Opened " << letter << " for read-only access.\n";
    if (diskSize_) std::cout << "    Device size: " << diskSize_ << " bytes\n";
    return true;
}

void DriveManager::Close() {
    if (hDrive != INVALID_HANDLE_VALUE) {
        CloseHandle(hDrive);
        hDrive = INVALID_HANDLE_VALUE;
    }
    diskSize_ = 0;
    bytesPerSector_ = 512;
}

bool DriveManager::ReadBytes(uint64_t offset, uint8_t* buffer, uint32_t size) {
    if (hDrive == INVALID_HANDLE_VALUE || (size != 0 && buffer == nullptr) ||
        offset > static_cast<uint64_t>(std::numeric_limits<LONGLONG>::max())) {
        return false;
    }
    if (diskSize_ != 0 &&
        (offset > diskSize_ || static_cast<uint64_t>(size) > diskSize_ - offset)) {
        return false;
    }
    if (size == 0) return true;

    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(hDrive, position, nullptr, FILE_BEGIN)) return false;

    DWORD bytesRead = 0;
    return ReadFile(hDrive, buffer, size, &bytesRead, nullptr) &&
           bytesRead == size;
}
