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
#define mix simd::mix
#endif

#define SpectrumYIntegral 106.856895f

enum : uint
{
    CIESpectrumWavelengthFirst = 360,
    CIESpectrumWavelengthLast = 830,
    CIESpectrumSampleCount = 471,
};

struct GPUSpectrum
{
    float samples[CIESpectrumSampleCount] = {};
};

inline GPUSpectrum GPUSpectrumConst(float t)
{
    GPUSpectrum result;
    for (uint i = 0; i < CIESpectrumSampleCount; ++i)
        result.samples[i] = t;
    return result;
}

inline float GPUSpectrumMax(const device GPUSpectrum& spectrum)
{
    float result = spectrum.samples[0];
    for (uint i = 1; i < CIESpectrumSampleCount; ++i)
        result = (result < spectrum.samples[i]) ? spectrum.samples[i] : result;
    return result;
}

inline float GPUSpectrumMax(const thread GPUSpectrum& spectrum)
{
    float result = spectrum.samples[0];
    for (uint i = 1; i < CIESpectrumSampleCount; ++i)
        result = (result < spectrum.samples[i]) ? spectrum.samples[i] : result;
    return result;
}

inline GPUSpectrum GPUSpectrumSqrt(const thread GPUSpectrum& spectrum)
{
    GPUSpectrum result = spectrum;
    for (uint i = 0; i < CIESpectrumSampleCount; ++i)
        result.samples[i] = result.samples[i] > 0.0f ? sqrt(result.samples[i]) : 0.0f;
    return result;
}

inline GPUSpectrum operator + (GPUSpectrum spectrum, float t) {
    for (uint i = 0; i < CIESpectrumSampleCount; ++i) spectrum.samples[i] += t;
    return spectrum;
}

inline GPUSpectrum operator - (GPUSpectrum spectrum, float t) {
    for (uint i = 0; i < CIESpectrumSampleCount; ++i) spectrum.samples[i] -= t;
    return spectrum;
}

inline GPUSpectrum operator * (GPUSpectrum spectrum, float t) {
    for (uint i = 0; i < CIESpectrumSampleCount; ++i) spectrum.samples[i] *= t;
    return spectrum;
}

inline GPUSpectrum operator / (GPUSpectrum spectrum, float t) {
    for (uint i = 0; i < CIESpectrumSampleCount; ++i) spectrum.samples[i] /= t;
    return spectrum;
}

inline GPUSpectrum operator + (GPUSpectrum spectrum, GPUSpectrum t) {
    for (uint i = 0; i < CIESpectrumSampleCount; ++i) spectrum.samples[i] += t.samples[i];
    return spectrum;
}

inline GPUSpectrum operator - (GPUSpectrum spectrum, GPUSpectrum t) {
    for (uint i = 0; i < CIESpectrumSampleCount; ++i) spectrum.samples[i] -= t.samples[i];
    return spectrum;
}

inline GPUSpectrum operator * (GPUSpectrum spectrum, GPUSpectrum t) {
    for (uint i = 0; i < CIESpectrumSampleCount; ++i) spectrum.samples[i] *= t.samples[i];
    return spectrum;
}

inline GPUSpectrum operator / (GPUSpectrum spectrum, GPUSpectrum t) {
    for (uint i = 0; i < CIESpectrumSampleCount; ++i) spectrum.samples[i] /= t.samples[i];
    return spectrum;
}

/*
inline float4 GPUSpectrumToXYZ(device const GPUSpectrum& spectrum, device const packed_float3* xyz)
{
    float3 result = { };
    for (uint i = 0; i < CIESpectrumSampleCount; ++i)
    {
        result.x += spectrum.samples[i] * xyz[i].x;
        result.y += spectrum.samples[i] * xyz[i].y;
        result.z += spectrum.samples[i] * xyz[i].z;
    }
    float scale = 1.0f / SpectrumYIntegral;
    return { result.x * scale, result.y * scale, result.z * scale, 1.0f };
}
*/

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

inline float GPUSpectrumSample(device const GPUSpectrum& spectrum, float wl)
{
    float wlFloor = floor(wl);
    float dwl = wl - wlFloor;
    uint w = uint(wlFloor) - CIESpectrumWavelengthFirst;
    return mix(spectrum.samples[w], spectrum.samples[w + 1], dwl);
}

inline float GPUSpectrumSample(thread const GPUSpectrum& spectrum, float wl)
{
    float wlFloor = floor(wl);
    float dwl = wl - wlFloor;
    uint w = uint(wlFloor) - CIESpectrumWavelengthFirst;
    return mix(spectrum.samples[w], spectrum.samples[w + 1], dwl);
}

#if defined(__METAL_VERSION__)
inline float4 GPUSpectrumToXYZ(float wavelength, float power, device const packed_float3* xyz)
{
    float wavelengthFloor = floor(wavelength);
    float dwl = wavelength - wavelengthFloor;
    uint w = uint(wavelengthFloor) - CIESpectrumWavelengthFirst;
    packed_float3 result = mix(xyz[w], xyz[w + 1], dwl);
    float scale = power / SpectrumYIntegral;
    return { result.x * scale, result.y * scale, result.z * scale, 1.0f };
}

inline float GPUSpectrumSample(constant const GPUSpectrum& spectrum, float wl)
{
    float wlFloor = floor(wl);
    float dwl = wl - wlFloor;
    uint w = uint(wlFloor) - CIESpectrumWavelengthFirst;
    return mix(spectrum.samples[w], spectrum.samples[w + 1], dwl);
}
#endif

#if !defined(__METAL_VERSION__)
#undef device
#undef thread
#endif
