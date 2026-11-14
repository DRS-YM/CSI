# FAT32 Digital Forensics & File Recovery Tool

A Windows-based C++17 command-line project for inspecting FAT32 volumes, listing directory entries (including entries marked as deleted), attempting file recovery, and producing per-case audit reports.

> **Project status:** Development / educational prototype. The repository has not been independently validated as a production forensic tool. Recovery results must be verified before they are relied upon.

## Overview

The application opens a selected Windows volume for read access, parses FAT32 filesystem metadata, scans directory entries, and offers interactive recovery options. For each recovery attempt, the application can calculate MD5 and SHA-256 digests for the source data and recovered output, then compare the results.

## Current capabilities visible in the source

- Open a Windows volume through the Win32 API using read access.
- Read and validate FAT32 boot-sector metadata and the selected active FAT.
- Parse directory entries and identify entries marked with the FAT deleted-entry marker.
- List deleted entries or all discovered files in the console.
- Attempt recovery of one, selected, or all deleted files.
- Calculate MD5 and SHA-256 digests through Windows CNG (BCrypt).
- Generate text, CSV, and JSON audit reports containing case details, recovery metadata, hashes, and integrity comparison results.

These are source-code capabilities, not a claim that every recovery scenario succeeds.

## Technology stack

| Area | Technology |
| --- | --- |
| Language | C++17 |
| Build system | CMake |
| Platform APIs | Windows API |
| Cryptography | Windows CNG / BCrypt |
| Filesystem format | FAT32 |
| Reports | TXT, CSV, JSON |

## Source layout

The project uses a conventional separation between public headers in `include/` and implementation files in `src/`:

| File | Responsibility |
| --- | --- |
| `src/main.cpp` | Application entry point, case details, component setup, and report generation |
| `include/DriveManager.h` and `src/DriveManager.cpp` | Opens a volume and reads raw bytes at specified offsets |
| `include/FAT32Analyzer.h` and `src/FAT32Analyzer.cpp` | Reads FAT32 metadata, walks directory entries, and attempts file extraction |
| `include/FileRecord.h` | Data model for discovered files and directories |
| `include/RecoveryEngine.h` and `src/RecoveryEngine.cpp` | Coordinates recovery, hashes source and destination data, and records results |
| `include/HashCalculator.h` and `src/HashCalculator.cpp` | Computes MD5 and SHA-256 hashes using Windows CNG |
| `include/AuditLogger.h` and `src/AuditLogger.cpp` | Stores recovery events and writes TXT, CSV, and JSON reports |
| `include/UI.h` and `src/UI.cpp` | Interactive console menu and file selection |
| `CMakeLists.txt` | CMake build configuration |
| `.gitignore` | Excludes generated build files and temporary artifacts |

See [docs/PROJECT_STRUCTURE.md](docs/PROJECT_STRUCTURE.md) for the component relationships, build configuration notes, and known technical limitations.

## Build status and setup

The project targets **Windows** and requires a C++17 compiler, CMake, and the Windows SDK.

The project source files are organized under `src/`, with public headers under `include/`. CMake is configured for this layout. A Windows Release build using MSVC and CMake completed successfully in the GitHub Actions workflow for this branch.

After the build paths are aligned, a typical CMake workflow is:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The executable needs sufficient Windows privileges to open a volume handle. Run only against storage devices you own or are explicitly authorized to examine. The [Windows build workflow](.github/workflows/windows-build.yml) automatically configures and builds the project on Windows for pushes and pull requests.

## Important forensic and recovery limitations

- **FAT32 only:** the parser explicitly checks for FAT32 metadata. NTFS, exFAT, and other filesystems are outside the current implementation.
- **Recovery is not guaranteed:** deleting a FAT32 directory entry may also release or alter its cluster-chain information. The current recovery path follows the FAT chain, so files whose chain has been cleared, reused, fragmented, or overwritten may be incomplete or unrecoverable.
- **No forensic write blocker is implemented:** the application requests read access to the source volume, but this is not a substitute for a hardware write blocker and a validated forensic acquisition workflow.
- **Hash comparison has a limited meaning:** matching source and destination hashes supports byte-for-byte integrity for the bytes actually read and recovered. It does not prove that the recovered file is complete, authentic, or forensically admissible.
- **MD5 is not collision-resistant:** it is included for compatibility/reporting; SHA-256 should be preferred for integrity verification.
- **Testing is not documented:** the repository currently contains no visible automated test suite or supplied test images. Validate with disposable FAT32 media and known test files before any important use.
- **Directory and filename handling is limited:** the current parser does not implement full FAT long-filename reconstruction.

## Safe testing recommendations

1. Use a disposable USB drive or a disk image containing non-sensitive test files.
2. Keep a separate backup of all test data.
3. Never experiment on the only copy of important data.
4. Compare recovered files with known originals using independent tools.
5. Record the test conditions and preserve the generated reports.

## Project purpose

This repository is suitable as a learning and development project exploring low-level disk access, FAT32 structures, file recovery concepts, hashing, and audit-report generation. It should not be represented as a certified forensic suite or used as the sole basis for legal or incident-response conclusions.

## License

No license file is currently present. All rights remain with the copyright holder unless a license is added.
