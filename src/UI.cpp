#include "UI.h"
#include <iostream>
#include <iomanip>
#include <limits>
#include <vector>

void UI::ListFiles(bool onlyDeleted) const {
    const auto& records = fs_.GetRecords();
    std::cout << "\n" << std::string(78, '=') << "\n"
              << std::left << std::setw(5) << "#"
              << std::setw(30) << "Name" << std::setw(12) << "Size"
              << std::setw(12) << "Cluster" << std::setw(10) << "Status" << "\n"
              << std::string(78, '-') << "\n";

    size_t shown = 0;
    for (size_t i = 0; i < records.size(); ++i) {
        const auto& record = records[i];
        if ((onlyDeleted && !record.isDeleted) || record.isDirectory) continue;
        std::cout << std::left << std::setw(5) << (i + 1)
                  << std::setw(30) << record.FullName().substr(0, 29)
                  << std::setw(12) << record.ReadableSize()
                  << std::setw(12) << record.startCluster
                  << std::setw(10) << (record.isDeleted ? "DELETED" : "OK") << "\n";
        ++shown;
    }
    std::cout << std::string(78, '=') << "\nTotal: " << shown << " file(s)\n";
}

void UI::HandleRecoverAll() {
    std::string output;
    std::cout << "Output directory: ";
    if (!std::getline(std::cin >> std::ws, output) || output.empty()) {
        std::cout << "[!] Output directory is required.\n";
        return;
    }
    engine_.RecoverAll(output);
}

void UI::HandleRecoverSelected() {
    std::cout << "Enter file numbers (one per line; 0 to finish):\n";
    std::vector<size_t> indices;
    while (true) {
        int value = 0;
        if (!(std::cin >> value)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "[!] Invalid number; selection cancelled.\n";
            return;
        }
        if (value == 0) break;
        if (value < 0) {
            std::cout << "[!] File numbers must be positive. Try again or enter 0.\n";
            continue;
        }
        indices.push_back(static_cast<size_t>(value - 1));
    }
    if (indices.empty()) {
        std::cout << "[i] No files selected.\n";
        return;
    }

    std::string output;
    std::cout << "Output directory: ";
    if (!std::getline(std::cin >> std::ws, output) || output.empty()) {
        std::cout << "[!] Output directory is required.\n";
        return;
    }
    engine_.RecoverMany(indices, output);
}

void UI::HandleRecoverByIndex() {
    int value = 0;
    std::cout << "File number: ";
    if (!(std::cin >> value) || value <= 0) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "[!] Enter a valid positive file number.\n";
        return;
    }

    std::string output;
    std::cout << "Output directory: ";
    if (!std::getline(std::cin >> std::ws, output) || output.empty()) {
        std::cout << "[!] Output directory is required.\n";
        return;
    }
    engine_.RecoverOne(static_cast<size_t>(value - 1), output);
}

void UI::ShowMenu() {
    while (true) {
        std::cout << "\n===== FAT32 File Recovery Tool =====\n"
                  << " 1) List deleted files\n"
                  << " 2) List all files\n"
                  << " 3) Recover all deleted files\n"
                  << " 4) Recover selected files\n"
                  << " 5) Recover one file by number\n"
                  << " 0) Exit\nChoice: ";
        int choice = -1;
        if (!(std::cin >> choice)) {
            if (std::cin.eof()) return;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "[!] Please enter a menu number.\n";
            continue;
        }

        switch (choice) {
            case 1: ListFiles(true); break;
            case 2: ListFiles(false); break;
            case 3: HandleRecoverAll(); break;
            case 4: HandleRecoverSelected(); break;
            case 5: HandleRecoverByIndex(); break;
            case 0: return;
            default: std::cout << "Invalid choice.\n";
        }
    }
}
