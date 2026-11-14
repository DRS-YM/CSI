#pragma once
#include <string>
#include <cstdint>
#include <cstdio>

struct FileRecord {
    std::string name;              // اسم الملف
    std::string extension;         // الامتداد
    uint64_t    size         = 0;
    uint32_t    startCluster = 0;
    uint32_t    parentCluster = 0;
    bool        isDeleted    = false;
    bool        isDirectory  = false;
    uint16_t    attributes   = 0;

    std::string FullName() const { return name + extension; }

    std::string ReadableSize() const {
        const char* units[] = {"B","KB","MB","GB","TB"};
        double s = static_cast<double>(size);
        int i = 0;
        while (s >= 1024.0 && i < 4) { s /= 1024.0; ++i; }
        char buf[64];
        snprintf(buf, sizeof(buf), "%.2f %s", s, units[i]);
        return buf;
    }
};