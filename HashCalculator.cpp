#include "HashCalculator.h"
#include <fstream>
#include <stdexcept>
#include <iostream>

LPCWSTR HashCalculator::AlgorithmName(HashAlgorithm algo) {
    switch (algo) {
        case HashAlgorithm::MD5:    return BCRYPT_MD5_ALGORITHM;
        case HashAlgorithm::SHA256: return BCRYPT_SHA256_ALGORITHM;
    }
    return BCRYPT_SHA256_ALGORITHM;
}

size_t HashCalculator::DigestSize(HashAlgorithm algo) {
    switch (algo) {
        case HashAlgorithm::MD5:    return 16;
        case HashAlgorithm::SHA256: return 32;
    }
    return 32;
}

std::string HashCalculator::ToHex(const std::vector<uint8_t>& data) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(data.size() * 2);
    for (uint8_t b : data) {
        out.push_back(hex[b >> 4]);
        out.push_back(hex[b & 0x0F]);
    }
    out.shrink_to_fit();
    return out;
}

HashCalculator::StreamHasher::StreamHasher(HashAlgorithm algo) : algo_(algo) {
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
            &algHandle_, AlgorithmName(algo), nullptr, 0))) {
        throw std::runtime_error("Cannot open BCrypt provider");
    }

    DWORD objSize = 0, cb = 0;
    BCryptGetProperty(algHandle_, BCRYPT_OBJECT_LENGTH,
                      reinterpret_cast<PUCHAR>(&objSize),
                      sizeof(objSize), &cb, 0);
    hashObject_.resize(objSize);

    BCryptCreateHash(algHandle_, &hashHandle_,
                     hashObject_.data(), objSize,
                     nullptr, 0, 0);
}

HashCalculator::StreamHasher::~StreamHasher() {
    if (hashHandle_) BCryptDestroyHash(hashHandle_);
    if (algHandle_)  BCryptCloseAlgorithmProvider(algHandle_, 0);
}

bool HashCalculator::StreamHasher::Update(const uint8_t* data, size_t size) {
    if (finalized_ || !hashHandle_) return false;
    NTSTATUS st = BCryptHashData(hashHandle_,
                                 const_cast<PUCHAR>(data),
                                 static_cast<ULONG>(size), 0);
    if (!BCRYPT_SUCCESS(st)) return false;
    totalBytes_ += size;
    return true;
}

HashResult HashCalculator::StreamHasher::Finalize() {
    HashResult result;
    result.algorithm      = algo_;
    result.bytesProcessed = totalBytes_;

    if (finalized_ || !hashHandle_) return result;

    std::vector<uint8_t> digest(DigestSize(algo_));
    NTSTATUS st = BCryptFinishHash(hashHandle_,
                                   digest.data(),
                                   static_cast<ULONG>(digest.size()), 0);
    finalized_ = true;

    if (!BCRYPT_SUCCESS(st)) return result;

    result.hexDigest = ToHex(digest);
    result.success   = true;
    return result;
}

HashResult HashCalculator::HashFile(const std::string& filePath,
                                    HashAlgorithm algo) {
    HashResult result;
    result.algorithm = algo;

    std::ifstream in(filePath, std::ios::binary);
    if (!in) return result;

    try {
        StreamHasher hasher(algo);
        constexpr size_t BUF_SIZE = 1 << 20;
        std::vector<uint8_t> buf(BUF_SIZE);

        while (in) {
            in.read(reinterpret_cast<char*>(buf.data()), BUF_SIZE);
            std::streamsize got = in.gcount();
            if (got > 0)
                if (!hasher.Update(buf.data(), static_cast<size_t>(got)))
                    return result;
        }
        return hasher.Finalize();
    }
    catch (...) { return result; }
}

HashResult HashCalculator::HashRawBytes(const uint8_t* data, uint64_t size,
                                        HashAlgorithm algo) {
    try {
        StreamHasher hasher(algo);
        hasher.Update(data, static_cast<size_t>(size));
        return hasher.Finalize();
    } catch (...) { return {}; }
}