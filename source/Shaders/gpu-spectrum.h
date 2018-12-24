//
//  gpu-spectrum.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "constants.h"

#if !defined(__METAL_VERSION__)
#define device volatile
#define thread
#endif

#define SpectrumYIntegral 106.856895f

enum : uint
{
    SpectrumWavelengthBegin = 360,
    SpectrumWavelengthEnd = 830,
    SpectrumSampleCount = (SpectrumWavelengthEnd - SpectrumWavelengthBegin) / 47
};

struct GPUSpectrum
{
    float samples[SpectrumSampleCount] = {};
};

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

inline GPUSpectrum GPUSpectrumSqrt(const thread GPUSpectrum& spectrum)
{
    GPUSpectrum result = spectrum;
    for (uint i = 0; i < SpectrumSampleCount; ++i)
        result.samples[i] = result.samples[i] > 0.0f ? sqrt(result.samples[i]) : 0.0f;
    return result;
}

inline GPUSpectrum operator + (GPUSpectrum spectrum, float t) {
    for (uint i = 0; i < SpectrumSampleCount; ++i) spectrum.samples[i] += t;
    return spectrum;
}

inline GPUSpectrum operator - (GPUSpectrum spectrum, float t) {
    for (uint i = 0; i < SpectrumSampleCount; ++i) spectrum.samples[i] -= t;
    return spectrum;
}

inline GPUSpectrum operator * (GPUSpectrum spectrum, float t) {
    for (uint i = 0; i < SpectrumSampleCount; ++i) spectrum.samples[i] *= t;
    return spectrum;
}

inline GPUSpectrum operator / (GPUSpectrum spectrum, float t) {
    for (uint i = 0; i < SpectrumSampleCount; ++i) spectrum.samples[i] /= t;
    return spectrum;
}

inline GPUSpectrum operator + (GPUSpectrum spectrum, GPUSpectrum t) {
    for (uint i = 0; i < SpectrumSampleCount; ++i) spectrum.samples[i] += t.samples[i];
    return spectrum;
}

inline GPUSpectrum operator - (GPUSpectrum spectrum, GPUSpectrum t) {
    for (uint i = 0; i < SpectrumSampleCount; ++i) spectrum.samples[i] -= t.samples[i];
    return spectrum;
}

inline GPUSpectrum operator * (GPUSpectrum spectrum, GPUSpectrum t) {
    for (uint i = 0; i < SpectrumSampleCount; ++i) spectrum.samples[i] *= t.samples[i];
    return spectrum;
}

inline GPUSpectrum operator / (GPUSpectrum spectrum, GPUSpectrum t) {
    for (uint i = 0; i < SpectrumSampleCount; ++i) spectrum.samples[i] /= t.samples[i];
    return spectrum;
}

inline float4 GPUSpectrumToXYZ(device const GPUSpectrum& spectrum, device const packed_float3* xyz)
{
    float3 result = { };
    for (uint i = 0; i < SpectrumSampleCount; ++i)
    {
        result.x += spectrum.samples[i] * xyz[i].x;
        result.y += spectrum.samples[i] * xyz[i].y;
        result.z += spectrum.samples[i] * xyz[i].z;
    }

    float scale = (float(SpectrumWavelengthEnd - SpectrumWavelengthBegin)) / (float(SpectrumSampleCount) * SpectrumYIntegral);
    return { result.x * scale, result.y * scale, result.z * scale, 1.0f };
}

inline float4 XYZtoRGB(float4 xyz)
{
    float r =  3.240479f * xyz[0] - 1.537150f * xyz[1] - 0.498535f * xyz[2];
    float g = -0.969256f * xyz[0] + 1.875991f * xyz[1] + 0.041556f * xyz[2];
    float b =  0.055648f * xyz[0] - 0.204043f * xyz[1] + 1.057311f * xyz[2];
    r = (r < 0.0f ? 0.0f : r);
    g = (g < 0.0f ? 0.0f : g);
    b = (b < 0.0f ? 0.0f : b);
    return { r, g, b, 1.0f };
}

#if !defined(__METAL_VERSION__)
#undef device
#undef thread
#endif
