#include "DriveManager.h"
#include <iostream>

DriveManager::DriveManager() : hDrive(INVALID_HANDLE_VALUE) {}

DriveManager::~DriveManager() { Close(); }

bool DriveManager::Open(const std::string& driveLetter) {
    Close();

    std::string path = "\\\\.\\" + driveLetter;

    hDrive = CreateFileA(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hDrive == INVALID_HANDLE_VALUE) {
        std::cerr << "[!] Error: Access Denied. Please run as Administrator.\n";
        return false;
    }

    DISK_GEOMETRY geo{};
    DWORD returned = 0;
    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                        nullptr, 0, &geo, sizeof(geo), &returned, nullptr)) {
        bytesPerSector_ = geo.BytesPerSector;
        diskSize_ = geo.Cylinders.QuadPart * geo.TracksPerCylinder *
                    geo.SectorsPerTrack * geo.BytesPerSector;
    }

    std::cout << "[+] Drive opened: " << driveLetter << "\n";
    return true;
}

void DriveManager::Close() {
    if (hDrive != INVALID_HANDLE_VALUE) {
        CloseHandle(hDrive);
        hDrive = INVALID_HANDLE_VALUE;
    }
}

bool DriveManager::ReadBytes(uint64_t offset, uint8_t* buffer, uint32_t size) {
    if (hDrive == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER liOffset;
    liOffset.QuadPart = static_cast<LONGLONG>(offset);

    if (!SetFilePointerEx(hDrive, liOffset, NULL, FILE_BEGIN))
        return false;

    DWORD bytesRead = 0;
    if (ReadFile(hDrive, buffer, size, &bytesRead, NULL) && bytesRead == size)
        return true;

    return false;
}