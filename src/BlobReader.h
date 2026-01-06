#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

struct BlobEntry
{
    std::string name;
    std::uint64_t offset = 0;
    std::uint64_t sizeBytes = 0;
};

struct BlobIndex
{
    std::uint32_t ownerId = 0;
    std::vector<BlobEntry> entries;
};

inline bool readU32(std::ifstream& f, std::uint32_t& v)
{
    f.read(reinterpret_cast<char*>(&v), sizeof(v));
    return f.good();
}

inline bool readU64(std::ifstream& f, std::uint64_t& v)
{
    f.read(reinterpret_cast<char*>(&v), sizeof(v));
    return f.good();
}

// Blob format (v1):
// magic[8] = "FATBLOB1"
// u32 ownerId
// u32 fileCount
// repeat fileCount:
//   u32 nameLen
//   name bytes (UTF-8)
//   u64 offset
//   u64 sizeBytes
//   u32 reserved
inline bool loadBlobIndex(const std::string& path, BlobIndex& out)
{
    out = {};
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    char magic[8] = {};
    f.read(magic, 8);
    if (!f) return false;

    const std::string m(magic, magic + 8);
    if (m != "FATBLOB1") return false;

    std::uint32_t ownerId = 0, fileCount = 0;
    if (!readU32(f, ownerId)) return false;
    if (!readU32(f, fileCount)) return false;

    out.ownerId = ownerId;
    out.entries.reserve(fileCount);

    for (std::uint32_t i = 0; i < fileCount; ++i)
    {
        std::uint32_t nameLen = 0;
        if (!readU32(f, nameLen)) return false;
        if (nameLen == 0 || nameLen > (1024u * 1024u)) return false;

        std::string name(nameLen, '\0');
        f.read(name.data(), nameLen);
        if (!f) return false;

        BlobEntry e;
        e.name = std::move(name);

        if (!readU64(f, e.offset)) return false;
        if (!readU64(f, e.sizeBytes)) return false;

        std::uint32_t reserved = 0;
        if (!readU32(f, reserved)) return false;

        out.entries.push_back(std::move(e));
    }

    return true;
}

inline bool readBlobPayload(const std::string& path, const BlobEntry& e, std::vector<std::uint8_t>& outBytes)
{
    outBytes.clear();

    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    f.seekg(static_cast<std::streamoff>(e.offset), std::ios::beg);
    if (!f) return false;

    if (e.sizeBytes == 0 || e.sizeBytes > (1024ull * 1024ull * 1024ull))
        return false; // 1GB sanity guard

    outBytes.resize(static_cast<size_t>(e.sizeBytes));
    f.read(reinterpret_cast<char*>(outBytes.data()), static_cast<std::streamsize>(e.sizeBytes));
    return f.good();
}