//
//  gpu-spectrum.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#if !defined(__METAL_VERSION__)
using uint = uint32_t;
#define device volatile
#define thread
#endif

enum : uint
{
    SpectrumWavelengthBegin = 360,
    SpectrumWavelengthEnd = 830,
    SpectrumSampleCount = 3 // (SpectrumWavelengthEnd - SpectrumWavelengthBegin) / 5
};

struct GPUSpectrum
{
    float samples[SpectrumSampleCount] = {};
};

inline GPUSpectrum GPUSpectrumFromRGB(float r, float g, float b)
{
    return { { r, g, b } };
}

inline GPUSpectrum GPUSpectrumFromRGB(float rgb[3])
{
    return GPUSpectrumFromRGB(rgb[0], rgb[1], rgb[2]);
}

inline GPUSpectrum GPUSpectrumFromRGB(const float rgb[3])
{
    return GPUSpectrumFromRGB(rgb[0], rgb[1], rgb[2]);
}

inline float GPUSpectrumLuminance(GPUSpectrum s)
{
    return s.samples[0] * 0.2126f + s.samples[1] * 0.7152f + s.samples[2] * 0.0722f;
}

inline GPUSpectrum GPUSpectrumConst(float t)
{
    GPUSpectrum result;
    for (uint i = 0; i < SpectrumSampleCount; ++i)
        result.samples[i] = t;
    return result;
}

inline float GPUSpectrumMax(const device GPUSpectrum& spectrum)
{
    float result = spectrum.samples[0];
    for (uint i = 1; i < SpectrumSampleCount; ++i)
        result = (result < spectrum.samples[i]) ? spectrum.samples[i] : result;
    return result;
}

inline float GPUSpectrumMax(const thread GPUSpectrum& spectrum)
{
    float result = spectrum.samples[0];
    for (uint i = 1; i < SpectrumSampleCount; ++i)
        result = (result < spectrum.samples[i]) ? spectrum.samples[i] : result;
    return result;
}

inline GPUSpectrum operator * (const device GPUSpectrum& spectrum, float t)
{
    GPUSpectrum result;
    for (uint i = 0; i < SpectrumSampleCount; ++i)
        result.samples[i] = spectrum.samples[i] * t;
    return result;
}

inline GPUSpectrum operator * (const thread GPUSpectrum& spectrum, float t)
{
    GPUSpectrum result;
    for (uint i = 0; i < SpectrumSampleCount; ++i)
        result.samples[i] = spectrum.samples[i] * t;
    return result;
}

inline GPUSpectrum operator * (const device GPUSpectrum& spectrum, const device GPUSpectrum& t)
{
    GPUSpectrum result;
    for (uint i = 0; i < SpectrumSampleCount; ++i)
        result.samples[i] = spectrum.samples[i] * t.samples[i];
    return result;
}

inline GPUSpectrum operator * (const device GPUSpectrum& spectrum, const thread GPUSpectrum& t)
{
    GPUSpectrum result;
    for (uint i = 0; i < SpectrumSampleCount; ++i)
        result.samples[i] = spectrum.samples[i] * t.samples[i];
    return result;
}

inline GPUSpectrum operator * (const thread GPUSpectrum& spectrum, const device GPUSpectrum& t)
{
    GPUSpectrum result = spectrum;
    for (uint i = 0; i < SpectrumSampleCount; ++i)
        result.samples[i] *= t.samples[i];
    return result;
}

inline GPUSpectrum operator * (const thread GPUSpectrum& spectrum, const thread GPUSpectrum& t)
{
    GPUSpectrum result = spectrum;
    for (uint i = 0; i < SpectrumSampleCount; ++i)
        result.samples[i] *= t.samples[i];
    return result;
}

inline GPUSpectrum operator + (const device GPUSpectrum& spectrum, const device GPUSpectrum& t)
{
    GPUSpectrum result;
    for (uint i = 0; i < SpectrumSampleCount; ++i)
        result.samples[i] = spectrum.samples[i] + t.samples[i];
    return result;
}

inline GPUSpectrum operator + (const device GPUSpectrum& spectrum, const thread GPUSpectrum& t)
{
    GPUSpectrum result;
    for (uint i = 0; i < SpectrumSampleCount; ++i)
        result.samples[i] = spectrum.samples[i] + t.samples[i];
    return result;
}

inline GPUSpectrum operator + (const thread GPUSpectrum& spectrum, const device GPUSpectrum& t)
{
    GPUSpectrum result = spectrum;
    for (uint i = 0; i < SpectrumSampleCount; ++i)
        result.samples[i] += t.samples[i];
    return result;
}

inline GPUSpectrum operator + (const thread GPUSpectrum& spectrum, const thread GPUSpectrum& t)
{
    GPUSpectrum result = spectrum;
    for (uint i = 0; i < SpectrumSampleCount; ++i)
        result.samples[i] += t.samples[i];
    return result;
}

#if !defined(__METAL_VERSION__)
#undef device
#undef thread
#endif
