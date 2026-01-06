#pragma once
#include <cmath>
#include <algorithm>

struct Voice
{
    bool active = false;
    int note = -1;
    float vel = 0.0f;

    double pos = 0.0;      // source frame position
    double step = 1.0;     // source frames per output sample

    bool looping = false;
    bool released = false;
    int fadeOutRemaining = 0;
};

static inline float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}