#pragma once
#include "FAT32Analyzer.h"
#include "RecoveryEngine.h"

class UI {
public:
    UI(FAT32Analyzer& fs, RecoveryEngine& engine)
        : fs_(fs), engine_(engine) {}

    void ShowMenu();
    void ListFiles(bool onlyDeleted = true) const;

private:
    FAT32Analyzer&  fs_;
    RecoveryEngine& engine_;

    void HandleRecoverAll();
    void HandleRecoverSelected();
    void HandleRecoverByIndex();
};