#pragma once

#include <cstdint>
#include <algorithm>
#include <cassert>
#include <cmath>
#include "Shaders/gpu-spectrum.h"

enum class RGBToSpectrumClass : uint32_t
{
    Reflectance,
    Illuminant,

    Count,
};

enum class RGBToSpectrumComponent : uint32_t
{
    White,
    Cyan,
    Magenta,
    Yellow,
    Red,
    Green,
    Blue,

    Count,
};

using Float3 = struct {
    float data[3]{};
    float& operator [](size_t i)
    {
        assert(i < 3);
        return data[i];
    }
};

class CIESpectrum
{
public:
    static void initialize();

    static CIESpectrum fromSamples(const float wavelengths[], const float values[], uint32_t count);
    static CIESpectrum fromGPUSpectrum(const GPUSpectrum&);
    static CIESpectrum fromRGB(RGBToSpectrumClass, const float[3]);
    static CIESpectrum fromBlackbodyWithTemperature(float t, bool normalized);

    /*
    static const CIESpectrum& X();
    static const CIESpectrum& Y();
    static const CIESpectrum& Z();
    */

public:
    explicit CIESpectrum(float val = 0.0f) {
        std::fill(std::begin(samples), std::end(samples), val);
    }

    Float3 toXYZ() const;
    Float3 toRGB() const;
    float toLuminance() const;
    GPUSpectrum toGPUSpectrum() const;

    void saturate(float lo, float hi);

    float& operator [](size_t i) { assert(i < CIESpectrumSampleCount); return samples[i]; }
    const float& operator [](size_t i) const { assert(i < CIESpectrumSampleCount); return samples[i]; }

    CIESpectrum& operator += (const CIESpectrum& r)
    {
        for (uint32_t i = 0; i < CIESpectrumSampleCount; ++i)
            samples[i] += r.samples[i];
        return (*this);
    }

    CIESpectrum& operator *= (float t)
    {
        for (uint32_t i = 0; i < CIESpectrumSampleCount; ++i)
            samples[i] *= t;
        return (*this);
    }

    CIESpectrum operator * (float t) const
    {
        CIESpectrum result = (*this);
        result *= t;
        return result;
    }

private:
    static float interpolateSamples(const float wavelengths[], const float values[], uint32_t count, float l0);
    static const CIESpectrum& RGBToSpectrum(RGBToSpectrumClass, RGBToSpectrumComponent);
    static const float yIntegral();

private:
    float samples[CIESpectrumSampleCount] = {};
};

inline CIESpectrum operator * (float t, const CIESpectrum& s) {
    return (s * t);
}

namespace CIE {
extern const float wavelengths[CIESpectrumSampleCount];
extern const float x[CIESpectrumSampleCount];
extern const float y[CIESpectrumSampleCount];
extern const float z[CIESpectrumSampleCount];

constexpr uint32_t RGBToSpectrum_Wavelength_Count = 32;
extern const float RGBToSpectrum_Wavelengths[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Reflectance_White[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Reflectance_Cyan[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Reflectance_Magenta[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Reflectance_Yellow[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Reflectance_Red[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Reflectance_Green[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Reflectance_Blue[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Illuminant_White[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Illuminant_Cyan[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Illuminant_Magenta[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Illuminant_Yellow[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Illuminant_Red[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Illuminant_Green[RGBToSpectrum_Wavelength_Count];
extern const float RGBToSpectrum_Illuminant_Blue[RGBToSpectrum_Wavelength_Count];
}

inline CIESpectrum CIESpectrum::fromSamples(const float wavelengths[], const float values[], uint32_t count)
{
    for (uint32_t i = 0; i + 1 < count; ++i)
    {
        assert(wavelengths[i] < wavelengths[i + 1]);
    }

    CIESpectrum result;
    for (uint32_t i = 0; i < CIESpectrumSampleCount; ++i)
    {
        float wl = float(CIESpectrumWavelengthFirst + i);
        result[i] =
            interpolateSamples(wavelengths, values, count, wl);
            // averageSamples(wavelengths, values, count, wl, wl + 1.0f);
    }
    return result;
}

inline CIESpectrum CIESpectrum::fromRGB(RGBToSpectrumClass cls, const float rgb[3])
{
    CIESpectrum result;
    if ((rgb[0] < rgb[1]) && (rgb[0] < rgb[2]))
    {
        result += rgb[0] * RGBToSpectrum(cls, RGBToSpectrumComponent::White);
        if (rgb[1] < rgb[2])
        {
            result += (rgb[1] - rgb[0]) * RGBToSpectrum(cls, RGBToSpectrumComponent::Cyan);
            result += (rgb[2] - rgb[1]) * RGBToSpectrum(cls, RGBToSpectrumComponent::Blue);
        }
        else
        {
            result += (rgb[2] - rgb[0]) * RGBToSpectrum(cls, RGBToSpectrumComponent::Cyan);
            result += (rgb[1] - rgb[2]) * RGBToSpectrum(cls, RGBToSpectrumComponent::Green);
        }
    }
    else if ((rgb[1] < rgb[0]) && (rgb[1] < rgb[2]))
    {
        result += rgb[1] * RGBToSpectrum(cls, RGBToSpectrumComponent::White);
        if (rgb[0] < rgb[2])
        {
            result += (rgb[0] - rgb[1]) * RGBToSpectrum(cls, RGBToSpectrumComponent::Magenta);
            result += (rgb[2] - rgb[0]) * RGBToSpectrum(cls, RGBToSpectrumComponent::Blue);
        }
        else
        {
            result += (rgb[2] - rgb[1]) * RGBToSpectrum(cls, RGBToSpectrumComponent::Magenta);
            result += (rgb[0] - rgb[2]) * RGBToSpectrum(cls, RGBToSpectrumComponent::Red);
        }
    }
    else
    {
        result += rgb[2] * RGBToSpectrum(cls, RGBToSpectrumComponent::White);
        if (rgb[0] < rgb[1])
        {
            result += (rgb[0] - rgb[2]) * RGBToSpectrum(cls, RGBToSpectrumComponent::Yellow);
            result += (rgb[1] - rgb[0]) * RGBToSpectrum(cls, RGBToSpectrumComponent::Green);
        }
        else
        {
            result += (rgb[1] - rgb[2]) * RGBToSpectrum(cls, RGBToSpectrumComponent::Yellow);
            result += (rgb[0] - rgb[1]) * RGBToSpectrum(cls, RGBToSpectrumComponent::Red);
        }
    }

    result *= (cls == RGBToSpectrumClass::Reflectance) ? (0.94f) : (0.86445f);

    result.saturate(0.0f, std::numeric_limits<float>::max());
    return result;
}

inline CIESpectrum CIESpectrum::fromBlackbodyWithTemperature(float temperature, bool normalized)
{
    static const float K1 = 1.1910427585e+19f; // 2 * h * c^2 / 10^-35
    static const float K2 = 1.4387751602e+5f; // h * c / k * 10^-7

    float leMax = 1.0f;
    if (normalized)
    {
        float wMax = (2.8977721e-3f / temperature) * 1.0e+7;
        leMax = K1 / (std::pow(wMax, 5.0f) * (std::exp(K2 / (wMax * temperature)) - 1.0f) * 1.0e+9f);
    }

    CIESpectrum result;
    for (uint32_t i = 0; i < CIESpectrumSampleCount; ++i)
    {
        float t = float(i) / float(CIESpectrumSampleCount - 1);
        float w = (float(CIESpectrumWavelengthFirst) * (1.0f - t) + float(CIESpectrumWavelengthLast) * t) / 100.0f;
        result.samples[i] = K1 / (std::pow(w, 5.0f) * (std::exp(K2 / (w * temperature)) - 1.0f) * 1.0e+9f);
        result.samples[i] /= leMax;
    }
    return result;
}

inline float CIESpectrum::interpolateSamples(const float wavelengths[], const float values[], uint32_t count, float wl)
{
    assert(count > 1);

    if (wl <= wavelengths[0])
        return values[0];

    if (wl >= wavelengths[count - 1])
        return values[count - 1];

    uint32_t i = 0;
    while (wl > wavelengths[i + 1]) {
        ++i;
    }

    float t = (wl - wavelengths[i]) / (wavelengths[i + 1] - wavelengths[i]);
    return values[i] * (1.0f - t) + values[i + 1] * t;
}

inline void CIESpectrum::initialize()
{
    // X();
    // Y();
    // Z();
    yIntegral();
    RGBToSpectrum(RGBToSpectrumClass::Reflectance, RGBToSpectrumComponent::White);
}

/*
inline const CIESpectrum& CIESpectrum::X()
{
    static const CIESpectrum x = CIESpectrum::fromSamples(CIE::wavelengths, CIE::x, CIESpectrumSampleCount);
    return x;
}

inline const CIESpectrum& CIESpectrum::Y()
{
    static const CIESpectrum y = CIESpectrum::fromSamples(CIE::wavelengths, CIE::y, CIESpectrumSampleCount);
    return y;
}

inline const CIESpectrum& CIESpectrum::Z()
{
    static const CIESpectrum z = CIESpectrum::fromSamples(CIE::wavelengths, CIE::z, CIESpectrumSampleCount);
    return z;
}
*/

inline const CIESpectrum& CIESpectrum::RGBToSpectrum(RGBToSpectrumClass cls, RGBToSpectrumComponent cmp)
{
    static const CIESpectrum c[uint32_t(RGBToSpectrumClass::Count)][uint32_t(RGBToSpectrumComponent::Count)] = {
        {
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Reflectance_White, CIE::RGBToSpectrum_Wavelength_Count),
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Reflectance_Cyan, CIE::RGBToSpectrum_Wavelength_Count),
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Reflectance_Magenta, CIE::RGBToSpectrum_Wavelength_Count),
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Reflectance_Yellow, CIE::RGBToSpectrum_Wavelength_Count),
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Reflectance_Red, CIE::RGBToSpectrum_Wavelength_Count),
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Reflectance_Green, CIE::RGBToSpectrum_Wavelength_Count),
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Reflectance_Blue, CIE::RGBToSpectrum_Wavelength_Count),
        },
        {
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Illuminant_White, CIE::RGBToSpectrum_Wavelength_Count),
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Illuminant_Cyan, CIE::RGBToSpectrum_Wavelength_Count),
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Illuminant_Magenta, CIE::RGBToSpectrum_Wavelength_Count),
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Illuminant_Yellow, CIE::RGBToSpectrum_Wavelength_Count),
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Illuminant_Red, CIE::RGBToSpectrum_Wavelength_Count),
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Illuminant_Green, CIE::RGBToSpectrum_Wavelength_Count),
            fromSamples(CIE::RGBToSpectrum_Wavelengths, CIE::RGBToSpectrum_Illuminant_Blue, CIE::RGBToSpectrum_Wavelength_Count),
        }
    };
    return c[uint32_t(cls)][uint32_t(cmp)];
}

inline const float CIESpectrum::yIntegral()
{
    auto acquireIntegral = []() -> float {
        float result = 0.0f;
        for (uint32_t i = 0; i < CIESpectrumSampleCount; ++i)
            result += CIE::y[i];
        return result;
    };
    static const float value = acquireIntegral();
    return value;
}

inline Float3 CIESpectrum::toXYZ() const
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    for (uint32_t i = 0; i < CIESpectrumSampleCount; ++i)
    {
        x += CIE::x[i] * samples[i];
        y += CIE::y[i] * samples[i];
        z += CIE::z[i] * samples[i];
    }
    float scale = 1.0 / yIntegral();
    return { x * scale, y * scale, z * scale };
}

inline float CIESpectrum::toLuminance() const
{
    float y = 0.0f;

    for (uint32_t i = 0; i < CIESpectrumSampleCount; ++i)
        y += CIE::y[i] * samples[i];

    float scale = 1.0f / yIntegral();
    return y * scale;
}

inline Float3 CIESpectrum::toRGB() const
{
    Float3 xyz = toXYZ();
    float r =  3.240479f * xyz[0] - 1.537150f * xyz[1] - 0.498535f * xyz[2];
    float g = -0.969256f * xyz[0] + 1.875991f * xyz[1] + 0.041556f * xyz[2];
    float b =  0.055648f * xyz[0] - 0.204043f * xyz[1] + 1.057311f * xyz[2];
    return { r, g, b };
}

inline void CIESpectrum::saturate(float lo, float hi)
{
    for (float& s : samples)
        s = std::max(lo, std::min(hi, s));
}

inline CIESpectrum CIESpectrum::fromGPUSpectrum(const GPUSpectrum& spectrum)
{
    CIESpectrum result;
    memcpy(result.samples, spectrum.samples, sizeof(spectrum.samples));
    return result;
}

inline GPUSpectrum CIESpectrum::toGPUSpectrum() const
{
    GPUSpectrum result;
    memcpy(result.samples, samples, sizeof(samples));
    return result;
}
