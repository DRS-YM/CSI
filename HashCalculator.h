#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

enum class HashAlgorithm { MD5, SHA256 };

struct HashResult {
    HashAlgorithm algorithm = HashAlgorithm::SHA256;
    std::string   hexDigest;
    uint64_t      bytesProcessed = 0;
    bool          success = false;
};

class HashCalculator {
public:
    static HashResult HashFile(const std::string& filePath, HashAlgorithm algo);
    static HashResult HashRawBytes(const uint8_t* data, uint64_t size,
                                   HashAlgorithm algo);

    class StreamHasher {
    public:
        explicit StreamHasher(HashAlgorithm algo);
        ~StreamHasher();
        bool Update(const uint8_t* data, size_t size);
        HashResult Finalize();

    private:
        BCRYPT_ALG_HANDLE    algHandle_  = nullptr;
        BCRYPT_HASH_HANDLE   hashHandle_ = nullptr;
        std::vector<uint8_t> hashObject_;
        HashAlgorithm        algo_;
        uint64_t             totalBytes_ = 0;
        bool                 finalized_  = false;
    };

private:
    static LPCWSTR AlgorithmName(HashAlgorithm algo);
    static size_t  DigestSize(HashAlgorithm algo);
    static std::string ToHex(const std::vector<uint8_t>& data);
};