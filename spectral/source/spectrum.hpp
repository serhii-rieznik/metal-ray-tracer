#pragma once

#include <cstdint>
#include <algorithm>
#include <cassert>

template <uint32_t sampleCount>
class SampledSpectrumBase
{
public:
    enum : uint32_t
    {
        SampleCount = sampleCount,
    };

public:
    explicit SampledSpectrumBase(float val = 0.0f)
    {
        std::fill(std::begin(samples), std::end(samples), val);
    }

    float& operator [](size_t i) { assert(i < SampleCount); return samples[i]; }
    const float& operator [](size_t i) const { assert(i < SampleCount); return samples[i]; }

    SampledSpectrumBase& operator += (const SampledSpectrumBase& r)
    {
        for (uint32_t i = 0; i < sampleCount; ++i)
            samples[i] += r.samples[i];

        return (*this);
    }

    SampledSpectrumBase& operator *= (float t)
    {
        for (uint32_t i = 0; i < sampleCount; ++i)
            samples[i] *= t;

        return (*this);
    }

    SampledSpectrumBase operator * (float t) const
    {
        SampledSpectrumBase result = (*this);
        result *= t;
        return result;
    }

protected:
    float samples[sampleCount] = {};
};

template <uint32_t sampleCount>
inline SampledSpectrumBase<sampleCount> operator * (float t, const SampledSpectrumBase<sampleCount>& s) {
    return (s * t);
}

enum : uint32_t
{
    WavelengthBegin = 400, // 360
    WavelengthEnd = 700, // 830
    SpectralSampleCount = 60
};

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

class SampledSpectrum : public SampledSpectrumBase<SpectralSampleCount>
{
public:
    static void initialize();

    static SampledSpectrum fromSamples(const float wavelengths[], const float values[], uint32_t count);
    static SampledSpectrum fromRGB(RGBToSpectrumClass, Float3);
    static SampledSpectrum fromBlackbodyWithTemperature(float t);

public:
    Float3 toXYZ() const;
    Float3 toRGB() const;

    void saturate(float lo, float hi);

private:
    static float averageSamples(const float wavelengths[], const float values[], uint32_t count, float l0, float l1);

    static const SampledSpectrum& X();
    static const SampledSpectrum& Y();
    static const SampledSpectrum& Z();

    static const SampledSpectrum& RGBToSpectrum(RGBToSpectrumClass, RGBToSpectrumComponent);
    static const float yIntegral();  
};

namespace CIE {
constexpr uint32_t sampleCount = 471;
extern const float wavelengths[sampleCount];
extern const float x[sampleCount];
extern const float y[sampleCount];
extern const float z[sampleCount];

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

inline SampledSpectrum SampledSpectrum::fromSamples(const float wavelengths[], const float values[], uint32_t count)
{
    for (uint32_t i = 0; i + 1 < count; ++i)
    {
        assert(wavelengths[i] < wavelengths[i + 1]);
    }

    SampledSpectrum result;
    for (uint32_t i = 0; i < SampleCount; ++i)
    {
        float t0 = float(i) / float(SampleCount);
        float l0 = float(WavelengthBegin) * (1.0f - t0) + float(WavelengthEnd) * t0;

        float t1 = float(i + 1) / float(SampleCount);
        float l1 = float(WavelengthBegin) * (1.0f - t1) + float(WavelengthEnd) * t1;
        
        result[i] = averageSamples(wavelengths, values, count, l0, l1);
    }
    return result;
}

inline SampledSpectrum SampledSpectrum::fromRGB(RGBToSpectrumClass cls, Float3 rgb)
{
    SampledSpectrum result = {};
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

    // result *= (cls == RGBToSpectrumClass::Reflectance) ? (0.94f) : (0.86445f);

    result.saturate(0.0f, std::numeric_limits<float>::max());
    return result;
}

inline SampledSpectrum SampledSpectrum::fromBlackbodyWithTemperature(float temperature)
{
    static const float K1 = 1.1910427585e+19f; // 2 * h * c^2 / 10^-35
    static const float K2 = 1.4387751602e+5f; // h * c / k * 10^-7

    SampledSpectrum result;
    for (uint32_t i = 0; i < SampleCount; ++i)
    {
        float t = float(i) / float(SampleCount - 1);
        float w = (float(WavelengthBegin) * (1.0f - t) + float(WavelengthEnd) * t) / 100.0f;
        result.samples[i] = K1 / (std::pow(w, 5.0f) * (std::exp(K2 / (w * temperature)) - 1.0f));
    }
    return result;
}

inline float SampledSpectrum::averageSamples(const float wavelengths[], const float values[], uint32_t count, float lBegin, float lEnd)
{
    assert(count > 0);

    if (count == 1)
        return values[0];

    if (lEnd <= wavelengths[0])
        return values[0];

    if (lBegin >= wavelengths[count - 1])
        return values[count - 1];

    float result = 0.0f;
    
    if (lBegin < wavelengths[0])
        result += 0.0f; //  values[0] * (wavelengths[0] - lBegin);

    if (lEnd > wavelengths[count - 1])
        result += 0.0f; //  values[count - 1] * (lEnd - wavelengths[count - 1]);

    uint32_t i = 0;
    while (lBegin > wavelengths[i + 1])
    {
        ++i;
    }

    auto interpolate = [wavelengths, values](float l, uint32_t i) {
        float t = (l - wavelengths[i]) / (wavelengths[i + 1] - wavelengths[i]);
        return values[i] * (1.0f - t) + values[i + 1] * t;
    };

    for (; (i + 1 < count) && (lEnd >= wavelengths[i]); ++i)
    {
        float l0 = std::max(lBegin, wavelengths[i]);
        float l1 = std::min(lEnd, wavelengths[i + 1]);
        result += 0.5f * (interpolate(l0, i) + interpolate(l1, i)) * (l1 - l0);
    }

    result *= 1.0f / (lEnd - lBegin);

    return result;
}

inline void SampledSpectrum::initialize()
{
    X();
    Y();
    Z();
    yIntegral();
    RGBToSpectrum(RGBToSpectrumClass::Reflectance, RGBToSpectrumComponent::White);
}

inline const SampledSpectrum& SampledSpectrum::X()
{
    static const SampledSpectrum x = SampledSpectrum::fromSamples(CIE::wavelengths, CIE::x, CIE::sampleCount);
    return x;
}

inline const SampledSpectrum& SampledSpectrum::Y()
{
    static const SampledSpectrum y = SampledSpectrum::fromSamples(CIE::wavelengths, CIE::y, CIE::sampleCount);
    return y;
}

inline const SampledSpectrum& SampledSpectrum::Z()
{
    static const SampledSpectrum z = SampledSpectrum::fromSamples(CIE::wavelengths, CIE::z, CIE::sampleCount);
    return z;
}

inline const SampledSpectrum& SampledSpectrum::RGBToSpectrum(RGBToSpectrumClass cls, RGBToSpectrumComponent cmp)
{
    static const SampledSpectrum c[uint32_t(RGBToSpectrumClass::Count)][uint32_t(RGBToSpectrumComponent::Count)] = {
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

inline const float SampledSpectrum::yIntegral()
{
    auto acquireIntegral = []() -> float {
        float result = 0.0f;
        for (uint32_t i = 0; i < CIE::sampleCount; ++i)
            result += CIE::y[i];
        return result;
    };
    static const float value = acquireIntegral();
    return value;
}

inline Float3 SampledSpectrum::toXYZ() const
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    const SampledSpectrum sX = X();
    const SampledSpectrum sY = Y();
    const SampledSpectrum sZ = Z();
    for (uint32_t i = 0; i < SampleCount; ++i)
    {
        x += sX.samples[i] * samples[i];
        y += sY.samples[i] * samples[i];
        z += sZ.samples[i] * samples[i];
    }
    float scale = (float(WavelengthEnd) - float(WavelengthBegin)) / (float(SampleCount) * yIntegral());
    return { x * scale, y * scale, z * scale };
}

inline Float3 SampledSpectrum::toRGB() const
{
    Float3 xyz = toXYZ();
    float r =  3.240479f * xyz[0] - 1.537150f * xyz[1] - 0.498535f * xyz[2];
    float g = -0.969256f * xyz[0] + 1.875991f * xyz[1] + 0.041556f * xyz[2];
    float b =  0.055648f * xyz[0] - 0.204043f * xyz[1] + 1.057311f * xyz[2];
    return { r, g, b };
}

inline void SampledSpectrum::saturate(float lo, float hi)
{
    for (float& s : samples)
        s = std::max(lo, std::min(hi, s));
}

