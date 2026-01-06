#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// Use the REAL SDK header from: D:\Workspace\FLOWSTONE_SDK\FlowStoneSdk.h
#include <FlowStoneSdk.h>

#include "BlobReader.h"
#include "WavReader.h"
#include "Voice.h"

static inline double midiNoteToRatio(int note, int root)
{
    const double semis = (double)(note - root);
    return std::pow(2.0, semis / 12.0);
}

static inline bool endsWithBlob(const std::string& s)
{
    if (s.size() < 5) return false;
    return _stricmp(s.c_str() + (s.size() - 5), ".blob") == 0;
}

struct Instance
{
    // Inputs (latched)
    int id = 0;
    std::string path;
    int index = 0;

    int root = 60;
    bool followPitch = true;
    bool loop = false;

    // Loaded sample
    bool sampleLoaded = false;
    WavData wav;

    // Voices
    static constexpr int kMaxVoices = 32;
    static constexpr int kFade = 64;
    Voice voices[kMaxVoices]{};

    void clearSample()
    {
        sampleLoaded = false;
        wav = {};
        for (auto& v : voices) v = {};
    }

    int allocVoice()
    {
        for (int i = 0; i < kMaxVoices; ++i)
            if (!voices[i].active) return i;

        int best = 0;
        double maxPos = voices[0].pos;
        for (int i = 1; i < kMaxVoices; ++i)
        {
            if (voices[i].pos > maxPos) { maxPos = voices[i].pos; best = i; }
        }
        return best;
    }

    void noteOn(int note, int velocity, double outSampleRate)
    {
        if (!sampleLoaded) return;
        if (velocity <= 0) return;

        const int vi = allocVoice();
        auto& v = voices[vi];
        v = {};
        v.active = true;
        v.note = note;
        v.vel = std::clamp((float)velocity / 127.0f, 0.0f, 1.0f);
        v.looping = loop;
        v.released = false;
        v.fadeOutRemaining = 0;
        v.pos = 0.0;

        const double pitchRatio = followPitch ? midiNoteToRatio(note, root) : 1.0;
        const double srRatio = (double)wav.sampleRate / std::max(1.0, outSampleRate);
        v.step = pitchRatio * srRatio;
    }

    void noteOff(int note)
    {
        for (auto& v : voices)
        {
            if (v.active && v.note == note)
            {
                v.released = true;
                v.fadeOutRemaining = kFade;
            }
        }
    }

    // MIDI expected as int-array bytes: [status,data1,data2]
    void handleMidiBytes(const int* bytes, unsigned len, double outSR)
    {
        if (!bytes || len < 1) return;

        auto handleOne = [&](int st, int d1, int d2)
        {
            const int status = st & 0xFF;
            const int type = status & 0xF0;

            const int note = d1 & 0x7F;
            const int vel = d2 & 0x7F;

            if (type == 0x90)
            {
                if (vel == 0) noteOff(note);
                else noteOn(note, vel, outSR);
            }
            else if (type == 0x80)
            {
                noteOff(note);
            }
        };

        if (len >= 3)
        {
            if ((len % 3) == 0)
            {
                for (unsigned i = 0; i + 2 < len; i += 3)
                    handleOne(bytes[i], bytes[i + 1], bytes[i + 2]);
            }
            else
            {
                handleOne(bytes[0], bytes[1], bytes[2]);
            }
        }
    }

    void render(float* outL, float* outR, unsigned nrSamples, double outSampleRate)
    {
        for (unsigned i = 0; i < nrSamples; i += 4)
        {
            outL[i] = 0.0f;
            outR[i] = 0.0f;
        }

        if (!sampleLoaded) return;

        const int frames = (int)wav.left.size();
        if (frames <= 0) return;

        const bool stereo = (wav.channels == 2) && (wav.right.size() == wav.left.size());
        const float* L = wav.left.data();
        const float* R = stereo ? wav.right.data() : nullptr;

        for (unsigned i = 0; i < nrSamples; i += 4)
        {
            float mixL = 0.0f;
            float mixR = 0.0f;

            for (auto& v : voices)
            {
                if (!v.active) continue;

                const double pitchRatio = followPitch ? midiNoteToRatio(v.note, root) : 1.0;
                const double srRatio = (double)wav.sampleRate / std::max(1.0, outSampleRate);
                v.step = pitchRatio * srRatio;

                const int i0 = (int)std::floor(v.pos);
                const int i1 = i0 + 1;
                const float t = (float)(v.pos - (double)i0);

                auto lerpLocal = [&](float a, float b, float tt) { return a + (b - a) * tt; };

                auto sampleAt = [&](const float* buf) -> float {
                    const int a = std::clamp(i0, 0, frames - 1);
                    const int b = std::clamp(i1, 0, frames - 1);
                    return lerpLocal(buf[a], buf[b], t);
                };

                const float sL = sampleAt(L);
                const float sR = stereo ? sampleAt(R) : 0.0f;

                float gain = v.vel;

                if (v.released)
                {
                    if (v.fadeOutRemaining > 0)
                    {
                        gain *= (float)v.fadeOutRemaining / (float)kFade;
                        v.fadeOutRemaining--;
                    }
                    else
                    {
                        v.active = false;
                        continue;
                    }
                }

                mixL += sL * gain;
                mixR += sR * gain;

                v.pos += v.step;

                if (v.pos >= (double)frames)
                {
                    if (v.looping && !v.released)
                        v.pos = std::fmod(v.pos, (double)frames);
                    else
                        v.active = false;
                }
            }

            outL[i] = mixL;
            outR[i] = mixR;
        }
    }
};

enum InPins
{
    PIN_ID = 0,
    PIN_PATH,
    PIN_INDEX,
    PIN_LOAD,
    PIN_CLEAR,
    PIN_ROOT,
    PIN_FOLLOW_PITCH,
    PIN_LOOP,
    PIN_MIDI_IN,
    NUM_INPUTS
};

enum OutPins
{
    OUT_L = 0,
    OUT_R,
    NUM_OUTPUTS
};

extern "C" __declspec(dllexport)
bool FsSdkMain(unsigned op, void* arg)
{
    switch (op)
    {
        case fsrInit:
            return true;

        case fsrDeInit:
            return true;

        case fsrDescribeModule:
        {
            auto* p = (FsSdkDescribeModule*)arg;
            p->sdkVersion = FS_SDK_VERSION;
            p->embed = FsSdkDescribeModule::embedMemory;

            FS_SDK_STRING(p->name, "Blob WAV Player");
            FS_SDK_STRING(p->desc, "Loads a WAV from a .blob container and plays it via MIDI bytes");
            FS_SDK_STRING(p->caption, "BlobWav");

            p->nrInputs = NUM_INPUTS;
            p->nrOutputs = NUM_OUTPUTS;
            p->timerMS = 0;
            return true;
        }

        case fsrDescribeIO:
        {
            const auto* p = (FsSdkDescribeIO*)arg;

            fsSdkDescribeInput(p, PIN_ID,           FsSdkDescribeIO::iotInt,      "id",           "Blob owner/protection id");
            fsSdkDescribeInput(p, PIN_PATH,         FsSdkDescribeIO::iotString,   "path",         "Full path to .blob file");
            fsSdkDescribeInput(p, PIN_INDEX,        FsSdkDescribeIO::iotInt,      "index",        "WAV index in blob");
            fsSdkDescribeInput(p, PIN_LOAD,         FsSdkDescribeIO::iotTrigger,  "load",         "Load WAV at index (clears previous)", true);
            fsSdkDescribeInput(p, PIN_CLEAR,        FsSdkDescribeIO::iotTrigger,  "clear",        "Clear sample and voices", true);
            fsSdkDescribeInput(p, PIN_ROOT,         FsSdkDescribeIO::iotInt,      "root",         "Root MIDI note (e.g. 48=C2)");
            fsSdkDescribeInput(p, PIN_FOLLOW_PITCH, FsSdkDescribeIO::iotBoolean,  "follow pitch", "Pitch follow across 128 notes");
            fsSdkDescribeInput(p, PIN_LOOP,         FsSdkDescribeIO::iotBoolean,  "loop",         "Loop while note is on");
            fsSdkDescribeInput(p, PIN_MIDI_IN,      FsSdkDescribeIO::iotIntArray, "midi in",      "MIDI bytes [status,data1,data2]");

            fsSdkDescribeOutput(p, OUT_L, FsSdkDescribeIO::iotMono, "L", "Left output (mono samples output here)");
            fsSdkDescribeOutput(p, OUT_R, FsSdkDescribeIO::iotMono, "R", "Right output (silent for mono samples)");
            return true;
        }

        case fsrCreateInstance:
        {
            auto* p = (FsSdkCreateInstance*)arg;
            p->pInstance = new Instance();
            return true;
        }

        case fsrDestroyInstance:
        {
            delete (Instance*)arg;
            return true;
        }

        case fsrTrigger:
        {
            const auto* t = (FsSdkTrigger*)arg;
            auto* inst = (Instance*)t->pInstance;
            if (!inst) return true;

            inst->id = fsSdkIntInput(t, PIN_ID);
            const char* pth = fsSdkStringInput(t, PIN_PATH);
            inst->path = pth ? pth : "";
            inst->index = fsSdkIntInput(t, PIN_INDEX);
            inst->root = fsSdkIntInput(t, PIN_ROOT);
            inst->followPitch = fsSdkBoolInput(t, PIN_FOLLOW_PITCH);
            inst->loop = fsSdkBoolInput(t, PIN_LOOP);

            const double outSR = t->timing(t->pOwner)->sampleRate;

            if (t->index >= FsSdkTrigger::tiFirstInput)
            {
                const int pin = (int)(t->index - FsSdkTrigger::tiFirstInput);

                if (pin == PIN_CLEAR)
                {
                    inst->clearSample();
                }
                else if (pin == PIN_LOAD)
                {
                    inst->clearSample();

                    if (!endsWithBlob(inst->path))
                        return true;

                    BlobIndex b;
                    if (!loadBlobIndex(inst->path, b))
                        return true;

                    if ((int)b.ownerId != inst->id)
                        return true;

                    if (inst->index < 0 || inst->index >= (int)b.entries.size())
                        return true;

                    std::vector<std::uint8_t> wavBytes;
                    if (!readBlobPayload(inst->path, b.entries[(size_t)inst->index], wavBytes))
                        return true;

                    WavData wd;
                    if (!parseWavPcm16or24(wavBytes, wd))
                        return true;

                    inst->wav = std::move(wd);
                    inst->sampleLoaded = true;
                }
                else if (pin == PIN_MIDI_IN)
                {
                    const auto* arr = (const FsSdkIntArray*)fsSdkInput(t, PIN_MIDI_IN);
                    if (!arr || !arr->pData || arr->len == 0)
                        return true;

                    inst->handleMidiBytes(arr->pData, arr->len, outSR);
                }
            }

            return true;
        }

        case fsrStream:
        {
            const auto* s = (FsSdkStream*)arg;
            auto* inst = (Instance*)s->pInstance;
            if (!inst) return true;

            float* outL = s->pOutputs[OUT_L];
            float* outR = s->pOutputs[OUT_R];

            inst->render(outL, outR, s->nrSamples, (double)s->pTiming->sampleRate);
            return true;
        }

        default:
            return true;
    }
}