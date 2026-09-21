#include "UI.h"
#include <iostream>
#include <iomanip>
#include <vector>

void UI::ListFiles(bool onlyDeleted) const {
    const auto& recs = fs_.GetRecords();

    std::cout << "\n" << std::string(78, '=') << "\n"
              << std::left
              << std::setw(5)  << "#"
              << std::setw(30) << "Name"
              << std::setw(12) << "Size"
              << std::setw(12) << "Cluster"
              << std::setw(10) << "Status" << "\n"
              << std::string(78, '-') << "\n";

    int shown = 0;
    for (size_t i = 0; i < recs.size(); ++i) {
        const auto& r = recs[i];
        if (onlyDeleted && !r.isDeleted) continue;
        if (r.isDirectory) continue;

        std::cout << std::left
                  << std::setw(5)  << (i + 1)
                  << std::setw(30) << r.FullName().substr(0, 29)
                  << std::setw(12) << r.ReadableSize()
                  << std::setw(12) << r.startCluster
                  << std::setw(10) << (r.isDeleted ? "DELETED" : "OK")
                  << "\n";
        ++shown;
    }
    std::cout << std::string(78, '=') << "\n"
              << "Total: " << shown << " file(s)\n";
}

void UI::HandleRecoverAll() {
    std::string out;
    std::cout << "Output directory (e.g. ./Recovered): ";
    std::cin >> out;
    engine_.RecoverAll(out);
}

void UI::HandleRecoverSelected() {
    std::cout << "Enter file numbers (space-separated, 0 to finish):\n";
    std::vector<size_t> ids;
    int x;
    while (std::cin >> x && x != 0) ids.push_back(x - 1);

    std::string out;
    std::cout << "Output directory: ";
    std::cin >> out;
    engine_.RecoverMany(ids, out);
}

void UI::HandleRecoverByIndex() {
    int idx;
    std::cout << "File #: ";
    std::cin >> idx;
    std::string out;
    std::cout << "Output directory: ";
    std::cin >> out;
    engine_.RecoverOne(idx - 1, out);
}

void UI::ShowMenu() {
    while (true) {
        std::cout << "\n===== File Recovery Tool (FAT32) =====\n"
                  << " 1) List deleted files\n"
                  << " 2) List all files\n"
                  << " 3) Recover ALL deleted files\n"
                  << " 4) Recover selected files\n"
                  << " 5) Recover single file by #\n"
                  << " 0) Exit\n"
                  << "Choice: ";

        int c;
        if (!(std::cin >> c)) { std::cin.clear(); std::cin.ignore(10000,'\n'); continue; }

        switch (c) {
            case 1: ListFiles(true);  break;
            case 2: ListFiles(false); break;
            case 3: HandleRecoverAll(); break;
            case 4: HandleRecoverSelected(); break;
            case 5: HandleRecoverByIndex(); break;
            case 0: return;
            default: std::cout << "Invalid choice.\n";
        }
    }
}