#include "context.hpp"

Context::Context()
{
    SampledSpectrum::initialize();
}

Context::~Context()
{

}

void Context::resize(uint32_t w, uint32_t h)
{
    _width = w;
    _height = h;
}

void Context::draw(HDC dc)
{
    const uint32_t wvCount = 1920;
    float wl[wvCount] = {};
    float val[wvCount] = {};

    for (uint32_t i = 0; i < wvCount; ++i)
    {
        float t = float(i) / float(wvCount);
        wl[i] = float(WavelengthBegin) * (1.0f - t) + float(WavelengthEnd) * t;
    }

    for (uint32_t i = 0; i < _width; ++i)
    {
        float a = float(i) / float(_width);
        float temperature = 1000.0f + 11000.0f * a;

        uint32_t vi = static_cast<uint32_t>(a * wvCount);
        float cwl = float(WavelengthBegin) * (1.0f - a) + float(WavelengthEnd) * a;

        for (uint32_t w = 0; w < wvCount; ++w)
        {
            float x = 1.0f - float(w) / float(wvCount);
            val[w] = 1.0f * std::pow(std::max(0.0f, 1.0f - std::abs(x - a)), 3.0f);
        }
        
        SampledSpectrum spectrum = 
            SampledSpectrum::fromBlackbodyWithTemperature(temperature);
            // SampledSpectrum::fromSamples(wl, val, wvCount);
        Float3 rgb = spectrum.toRGB();

        SampledSpectrum fromRGB = SampledSpectrum::fromRGB(RGBToSpectrumClass::Illuminant, rgb);
        rgb = fromRGB.toRGB();

        float mx = std::max(rgb[0], std::max(rgb[1], rgb[2]));

        rgb[0] = std::max(0.0f, std::min(1.0f, rgb[0] / mx));
        rgb[1] = std::max(0.0f, std::min(1.0f, rgb[1] / mx));
        rgb[2] = std::max(0.0f, std::min(1.0f, rgb[2] / mx));

        //*
        rgb[0] = std::pow(rgb[0], 1.0f / 2.2f);
        rgb[1] = std::pow(rgb[1], 1.0f / 2.2f);
        rgb[2] = std::pow(rgb[2], 1.0f / 2.2f);
        // */

        uint8_t r = static_cast<uint8_t>(255.0 * rgb[0]);
        uint8_t g = static_cast<uint8_t>(255.0 * rgb[1]);
        uint8_t b = static_cast<uint8_t>(255.0 * rgb[2]);
        HPEN pen = CreatePen(PS_SOLID, 5, RGB(r, g, b));
        POINT pt = {};

        HGDIOBJ currentPen = SelectObject(dc, pen);
        MoveToEx(dc, i, 0, &pt);
        LineTo(dc, i, _height);

        SelectObject(dc, currentPen);
        DeleteObject(pen);
    }
}
