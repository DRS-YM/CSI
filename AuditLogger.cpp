#include "AuditLogger.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <fstream>

namespace fs = std::filesystem;

AuditLogger::AuditLogger(const std::string& caseId,
                         const std::string& examinerName,
                         const std::string& driveLetter,
                         const std::string& outputDir)
    : caseId_(caseId), examinerName_(examinerName),
      driveLetter_(driveLetter), outputDir_(outputDir) {
    timestamp_ = TimestampString(std::chrono::system_clock::now());
    for (auto& c : timestamp_) if (c == ':' || c == ' ') c = '_';
    fs::create_directories(outputDir_);
}

std::string AuditLogger::TimestampString(
    std::chrono::system_clock::time_point tp) const {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string AuditLogger::SafeFilePrefix() const {
    return "Case_" + caseId_ + "_" + timestamp_;
}

std::string AuditLogger::EscapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

std::string AuditLogger::EscapeCsv(const std::string& s) {
    if (s.find_first_of(",\"\n") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) { if (c == '"') out += "\"\""; else out += c; }
    out += "\"";
    return out;
}

void AuditLogger::AddEntry(const RecoveryLogEntry& e) { entries_.push_back(e); }

bool AuditLogger::WriteTextReport() {
    std::string path = (fs::path(outputDir_) /
                        (SafeFilePrefix() + "_report.txt")).string();
    std::ofstream f(path);
    if (!f) return false;

    f << "==========================================================\n";
    f << "         DIGITAL FORENSICS - CHAIN OF CUSTODY REPORT      \n";
    f << "==========================================================\n";
    f << "Case ID        : " << caseId_ << "\n";
    f << "Examiner       : " << examinerName_ << "\n";
    f << "Source Drive   : " << driveLetter_ << "\n";
    f << "Report Time    : " << TimestampString(
            std::chrono::system_clock::now()) << "\n";
    f << "Tool           : FileRecoveryTool v1.0\n";
    f << "Algorithms     : SHA-256 + MD5 (Windows CNG)\n";
    f << "Total Files    : " << entries_.size() << "\n";
    f << "----------------------------------------------------------\n\n";

    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        f << "[" << (i + 1) << "] " << e.fileName << "\n"
          << "    Size           : " << e.fileSize << " bytes\n"
          << "    Start Cluster  : " << e.startCluster << "\n"
          << "    Was Deleted    : " << (e.wasDeleted ? "YES" : "NO") << "\n"
          << "    Source MD5     : " << e.sourceMd5 << "\n"
          << "    Source SHA-256 : " << e.sourceSha256 << "\n"
          << "    Dest   MD5     : " << e.destMd5 << "\n"
          << "    Dest   SHA-256 : " << e.destSha256 << "\n"
          << "    Integrity      : "
          << (e.integrityOk ? "VERIFIED" : "FAILED") << "\n"
          << "    Recovered To   : " << e.recoveredPath << "\n"
          << "    Recovery Time  : " << TimestampString(e.recoveryTime) << "\n"
          << "    Elapsed        : " << e.elapsedMs << " ms\n"
          << "----------------------------------------------------------\n";
    }

    std::cout << "[+] Text report: " << path << "\n";
    return true;
}

bool AuditLogger::WriteCsvReport() {
    std::string path = (fs::path(outputDir_) /
                        (SafeFilePrefix() + "_report.csv")).string();
    std::ofstream f(path);
    if (!f) return false;

    f << "Index,FileName,Size,StartCluster,WasDeleted,"
         "SourceMD5,SourceSHA256,DestMD5,DestSHA256,"
         "IntegrityOK,RecoveredPath,RecoveryTime,ElapsedMs\n";

    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        f << (i + 1) << "," << EscapeCsv(e.fileName) << ","
          << e.fileSize << "," << e.startCluster << ","
          << (e.wasDeleted ? "true" : "false") << ","
          << e.sourceMd5 << "," << e.sourceSha256 << ","
          << e.destMd5 << "," << e.destSha256 << ","
          << (e.integrityOk ? "true" : "false") << ","
          << EscapeCsv(e.recoveredPath) << ","
          << TimestampString(e.recoveryTime) << ","
          << e.elapsedMs << "\n";
    }
    std::cout << "[+] CSV  report: " << path << "\n";
    return true;
}

bool AuditLogger::WriteJsonReport() {
    std::string path = (fs::path(outputDir_) /
                        (SafeFilePrefix() + "_report.json")).string();
    std::ofstream f(path);
    if (!f) return false;

    f << "{\n"
      << "  \"caseId\": \"" << EscapeJson(caseId_) << "\",\n"
      << "  \"examiner\": \"" << EscapeJson(examinerName_) << "\",\n"
      << "  \"drive\": \"" << EscapeJson(driveLetter_) << "\",\n"
      << "  \"reportTime\": \""
      << TimestampString(std::chrono::system_clock::now()) << "\",\n"
      << "  \"tool\": \"FileRecoveryTool v1.0\",\n"
      << "  \"totalFiles\": " << entries_.size() << ",\n"
      << "  \"entries\": [\n";

    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        f << "    {\n"
          << "      \"index\": " << (i + 1) << ",\n"
          << "      \"fileName\": \"" << EscapeJson(e.fileName) << "\",\n"
          << "      \"size\": " << e.fileSize << ",\n"
          << "      \"startCluster\": " << e.startCluster << ",\n"
          << "      \"wasDeleted\": " << (e.wasDeleted ? "true":"false") << ",\n"
          << "      \"sourceMd5\": \"" << e.sourceMd5 << "\",\n"
          << "      \"sourceSha256\": \"" << e.sourceSha256 << "\",\n"
          << "      \"destMd5\": \"" << e.destMd5 << "\",\n"
          << "      \"destSha256\": \"" << e.destSha256 << "\",\n"
          << "      \"integrityOk\": " << (e.integrityOk?"true":"false") << ",\n"
          << "      \"recoveredPath\": \"" << EscapeJson(e.recoveredPath) << "\",\n"
          << "      \"recoveryTime\": \"" << TimestampString(e.recoveryTime) << "\",\n"
          << "      \"elapsedMs\": " << e.elapsedMs << "\n"
          << "    }" << (i + 1 < entries_.size() ? "," : "") << "\n";
    }
    f << "  ]\n}\n";
    std::cout << "[+] JSON report: " << path << "\n";
    return true;
}

void AuditLogger::PrintSummary() const {
    size_t ok = 0, bad = 0;
    for (const auto& e : entries_) e.integrityOk ? ++ok : ++bad;
    std::cout << "\n========== Audit Summary ==========\n"
              << "Case ID         : " << caseId_ << "\n"
              << "Examiner        : " << examinerName_ << "\n"
              << "Total recovered : " << entries_.size() << "\n"
              << "Integrity OK    : " << ok << "\n"
              << "Integrity FAIL  : " << bad << "\n"
              << "===================================\n";
}