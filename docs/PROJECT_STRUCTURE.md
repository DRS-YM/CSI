# Project Structure and Component Guide

This document explains the current source layout and the role of each component.

## Application flow

```text
main.cpp
  ├── DriveManager       Opens the Windows volume and reads raw byte ranges
  ├── FAT32Analyzer      Parses FAT32 metadata and enumerates directory entries
  │     └── FileRecord   Holds metadata for each discovered entry
  ├── RecoveryEngine     Coordinates recovery and integrity checks
  │     ├── HashCalculator  Computes MD5 and SHA-256 digests
  │     └── AuditLogger     Writes case and recovery reports
  └── UI                 Provides the interactive console menu
```

## File reference

| File | Purpose |
| --- | --- |
| `main.cpp` | Collects case details, initializes the components, runs the menu, and writes reports |
| `DriveManager.h/.cpp` | Manages the raw Windows volume handle and offset-based reads |
| `FAT32Analyzer.h/.cpp` | Reads the boot sector and FAT, scans directories, and attempts file extraction |
| `FileRecord.h` | Represents a discovered file or directory and formats its size |
| `RecoveryEngine.h/.cpp` | Selects files for recovery, hashes source and output bytes, and records results |
| `HashCalculator.h/.cpp` | Streams file data through Windows CNG to calculate MD5 or SHA-256 |
| `AuditLogger.h/.cpp` | Stores recovery events and writes text, CSV, and JSON reports |
| `UI.h/.cpp` | Lists entries and handles the user's recovery selections |
| `CMakeLists.txt` | Defines the Windows-only C++17 build |
| `.gitignore` | Excludes build outputs and temporary files |

## Build configuration correction

The source files are currently stored in the repository root. The previous CMake configuration pointed to a non-existent `src/` folder and `include/` directory. CMake now compiles the root-level source files, links Windows BCrypt, and defines `NOMINMAX` to prevent Win32 macro collisions.

Build on Windows with CMake and a supported C++17 toolchain:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

A Windows Release build using MSVC completed successfully through the GitHub Actions workflow. This confirms compilation and linking; it does not replace runtime tests against FAT32 media or disk images.

## Important implementation limitations

1. **Deleted-file recovery:** FAT32 may clear or reuse a deleted file's cluster chain. The current implementation follows the FAT chain and can therefore fail or produce incomplete output when chain metadata is unavailable.
2. **Fragmentation:** recovering a deleted file by assuming adjacent clusters would only be a heuristic and is not implemented as a validated recovery method.
3. **Long filenames:** the directory parser skips Long File Name (LFN) entries; it currently uses short 8.3 names.
4. **Directory paths:** the scanner receives a parent path, but the current `FileRecord` does not retain a complete reconstructed path.
5. **Forensic acquisition:** opening a volume for read access is not equivalent to using a hardware write blocker or creating a verified forensic image.
6. **Validation:** no automated test suite or known-good disk-image fixtures are currently included.

## Suggested next engineering steps

- Add a read-only disk-image input mode so tests do not need a physical drive.
- Add bounds checks for all BPB-derived offsets and cluster numbers.
- Detect FAT32 active-FAT settings and handle multiple FAT copies correctly.
- Add cycle detection and recursion-depth limits during directory traversal.
- Implement LFN reconstruction and preserve full relative paths.
- Add a recovery strategy for deleted entries with unavailable FAT chains, clearly labeling heuristic results.
- Add unit tests using disposable FAT32 images and known file hashes.
- Add richer recovery diagnostics and record every failure path in the audit log.

## Safety

Use only media you own or are explicitly authorized to examine. Test with disposable media or disk images and retain a separate backup. Do not treat the output as legally admissible evidence without a validated forensic process.
