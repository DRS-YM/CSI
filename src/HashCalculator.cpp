#include "HashCalculator.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>

LPCWSTR HashCalculator::AlgorithmName(HashAlgorithm algo) {
    switch (algo) {
        case HashAlgorithm::MD5: return BCRYPT_MD5_ALGORITHM;
        case HashAlgorithm::SHA256: return BCRYPT_SHA256_ALGORITHM;
    }
    return BCRYPT_SHA256_ALGORITHM;
}

size_t HashCalculator::DigestSize(HashAlgorithm algo) {
    return algo == HashAlgorithm::MD5 ? 16u : 32u;
}

std::string HashCalculator::ToHex(const std::vector<uint8_t>& data) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(data.size() * 2);
    for (uint8_t byte : data) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0F]);
    }
    return result;
}

HashCalculator::StreamHasher::StreamHasher(HashAlgorithm algo) : algo_(algo) {
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algHandle_, AlgorithmName(algo), nullptr, 0);
    if (!BCRYPT_SUCCESS(status))
        throw std::runtime_error("BCryptOpenAlgorithmProvider failed");

    DWORD objectLength = 0, resultLength = 0;
    status = BCryptGetProperty(algHandle_, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength),
        &resultLength, 0);
    if (!BCRYPT_SUCCESS(status) || objectLength == 0) {
        BCryptCloseAlgorithmProvider(algHandle_, 0);
        algHandle_ = nullptr;
        throw std::runtime_error("BCryptGetProperty(BCRYPT_OBJECT_LENGTH) failed");
    }

    hashObject_.resize(objectLength);
    status = BCryptCreateHash(algHandle_, &hashHandle_, hashObject_.data(),
        objectLength, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(algHandle_, 0);
        algHandle_ = nullptr;
        hashObject_.clear();
        throw std::runtime_error("BCryptCreateHash failed");
    }
}

HashCalculator::StreamHasher::~StreamHasher() {
    if (hashHandle_) BCryptDestroyHash(hashHandle_);
    if (algHandle_) BCryptCloseAlgorithmProvider(algHandle_, 0);
}

bool HashCalculator::StreamHasher::Update(const uint8_t* data, size_t size) {
    if (finalized_ || !hashHandle_ || (size && !data) ||
        size > std::numeric_limits<ULONG>::max()) return false;
    if (size == 0) return true;

    const NTSTATUS status = BCryptHashData(hashHandle_,
        const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0);
    if (!BCRYPT_SUCCESS(status)) return false;
    totalBytes_ += size;
    return true;
}

HashResult HashCalculator::StreamHasher::Finalize() {
    HashResult result;
    result.algorithm = algo_;
    result.bytesProcessed = totalBytes_;
    if (finalized_ || !hashHandle_) return result;

    std::vector<uint8_t> digest(DigestSize(algo_));
    const NTSTATUS status = BCryptFinishHash(hashHandle_, digest.data(),
        static_cast<ULONG>(digest.size()), 0);
    finalized_ = true;
    if (!BCRYPT_SUCCESS(status)) return result;

    result.hexDigest = ToHex(digest);
    result.success = true;
    return result;
}

HashResult HashCalculator::HashFile(const std::string& filePath,
                                    HashAlgorithm algo) {
    HashResult failure;
    failure.algorithm = algo;
    std::ifstream input(filePath, std::ios::binary);
    if (!input) return failure;

    try {
        StreamHasher hasher(algo);
        constexpr size_t bufferSize = 1024 * 1024;
        std::vector<uint8_t> buffer(bufferSize);
        while (input) {
            input.read(reinterpret_cast<char*>(buffer.data()),
                       static_cast<std::streamsize>(buffer.size()));
            const std::streamsize count = input.gcount();
            if (count > 0 &&
                !hasher.Update(buffer.data(), static_cast<size_t>(count)))
                return failure;
        }
        if (input.bad()) return failure;
        return hasher.Finalize();
    } catch (...) {
        return failure;
    }
}

HashResult HashCalculator::HashRawBytes(const uint8_t* data, uint64_t size,
                                        HashAlgorithm algo) {
    HashResult failure;
    failure.algorithm = algo;
    if ((size != 0 && data == nullptr) ||
        size > std::numeric_limits<size_t>::max()) return failure;
    try {
        StreamHasher hasher(algo);
        if (!hasher.Update(data, static_cast<size_t>(size))) return failure;
        return hasher.Finalize();
    } catch (...) {
        return failure;
    }
}
