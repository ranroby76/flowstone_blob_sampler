#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <algorithm>

struct WavData
{
    int sampleRate = 0;
    int channels = 0;          // 1 or 2
    int bitsPerSample = 0;     // 16 or 24
    std::vector<float> left;   // deinterleaved float32
    std::vector<float> right;  // empty if mono
};

static inline std::uint32_t rd32(const std::uint8_t* p)
{
    return (std::uint32_t)p[0]
        | ((std::uint32_t)p[1] << 8)
        | ((std::uint32_t)p[2] << 16)
        | ((std::uint32_t)p[3] << 24);
}

static inline std::uint16_t rd16(const std::uint8_t* p)
{
    return (std::uint16_t)p[0] | ((std::uint16_t)p[1] << 8);
}

// Supports PCM 16-bit or 24-bit, SR <= 48000, mono/stereo only.
inline bool parseWavPcm16or24(const std::vector<std::uint8_t>& bytes, WavData& out)
{
    out = {};
    if (bytes.size() < 44) return false;
    if (std::memcmp(bytes.data(), "RIFF", 4) != 0) return false;
    if (std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) return false;

    size_t pos = 12;

    bool hasFmt = false;
    bool hasData = false;

    std::uint16_t audioFormat = 0;
    std::uint16_t numCh = 0;
    std::uint32_t sr = 0;
    std::uint16_t bps = 0;

    const std::uint8_t* dataPtr = nullptr;
    std::uint32_t dataSize = 0;

    while (pos + 8 <= bytes.size())
    {
        const char* ckId = (const char*)(bytes.data() + pos);
        std::uint32_t ckSize = rd32(bytes.data() + pos + 4);
        pos += 8;

        if (pos + ckSize > bytes.size())
            return false;

        if (std::memcmp(ckId, "fmt ", 4) == 0)
        {
            if (ckSize < 16) return false;
            audioFormat = rd16(bytes.data() + pos + 0);
            numCh       = rd16(bytes.data() + pos + 2);
            sr          = rd32(bytes.data() + pos + 4);
            bps         = rd16(bytes.data() + pos + 14);
            hasFmt = true;
        }
        else if (std::memcmp(ckId, "data", 4) == 0)
        {
            dataPtr = bytes.data() + pos;
            dataSize = ckSize;
            hasData = true;
        }

        // word aligned
        pos += ckSize + (ckSize & 1u);
    }

    if (!hasFmt || !hasData) return false;
    if (audioFormat != 1) return false; // PCM
    if (!(numCh == 1 || numCh == 2)) return false;
    if (!(bps == 16 || bps == 24)) return false;
    if (sr == 0 || sr > 48000) return false;

    const int bytesPerSample = (bps == 16) ? 2 : 3;
    const int frameBytes = bytesPerSample * (int)numCh;
    if (frameBytes <= 0) return false;
    if ((dataSize % (std::uint32_t)frameBytes) != 0) return false;

    const std::uint32_t frames = dataSize / (std::uint32_t)frameBytes;

    out.sampleRate = (int)sr;
    out.channels = (int)numCh;
    out.bitsPerSample = (int)bps;
    out.left.resize(frames);
    if (numCh == 2) out.right.resize(frames);

    auto read16 = [&](const std::uint8_t* p) -> float {
        const int16_t s = (int16_t)rd16(p);
        return (float)s / 32768.0f;
    };
    auto read24 = [&](const std::uint8_t* p) -> float {
        int32_t v = (int32_t)p[0] | ((int32_t)p[1] << 8) | ((int32_t)p[2] << 16);
        if (v & 0x00800000) v |= 0xFF000000;
        return (float)v / 8388608.0f;
    };

    const std::uint8_t* p = dataPtr;

    for (std::uint32_t i = 0; i < frames; ++i)
    {
        float L = 0.f, R = 0.f;

        if (bps == 16)
        {
            L = read16(p); p += 2;
            if (numCh == 2) { R = read16(p); p += 2; }
        }
        else
        {
            L = read24(p); p += 3;
            if (numCh == 2) { R = read24(p); p += 3; }
        }

        out.left[i] = L;
        if (numCh == 2) out.right[i] = R;
    }

    return true;
}