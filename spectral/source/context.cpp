#include "context.hpp"

Context::Context()
{
    SampledSpectrum::X();
    SampledSpectrum::Y();
    SampledSpectrum::Z();
    SampledSpectrum::yIntegral();
}

Context::~Context()
{

}

void Context::resize(uint32_t w, uint32_t h)
{
    _width = w;
    _height = h;
}

float bb(float w, float t)
{
    const float K1 = 1.1910427585e+19f; // 2 * h * c^2 / 10^-35
    const float K2 = 1.4387751602e+5f; // h * c / k * 10^-7
    w /= 100.0f;
    float wl5 = w * (w * w) * (w * w);
    return K1 / (wl5 * (std::exp(K2 / (w * t)) - 1.0f));
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
        uint32_t vi(a * wvCount);
        float cwl = float(WavelengthBegin) * (1.0f - a) + float(WavelengthEnd) * a;

        for (uint32_t w = 0; w < wvCount; ++w)
        {
            float x = 1.0f - float(w) / float(wvCount);
            val[w] = 15.0f * (1.0f - std::pow(std::abs(x - a), 1.0f / 64.0f));
            // val[w] = bb(wl[w], 1000.0f + 21000.0f * a);
        }
        
        SampledSpectrum spectrum = SampledSpectrum::fromSamples(wl, val, wvCount);
        Float3 rgb = spectrum.toRGB();

        float mx = 1.0f; // std::max(rgb.a, std::max(rgb.b, rgb.c));

        rgb.a = std::max(0.0f, std::min(1.0f, rgb.a / mx));
        rgb.b = std::max(0.0f, std::min(1.0f, rgb.b / mx));
        rgb.c = std::max(0.0f, std::min(1.0f, rgb.c / mx));

        rgb.a = std::pow(rgb.a, 1.0f / 2.2f);
        rgb.b = std::pow(rgb.b, 1.0f / 2.2f);
        rgb.c = std::pow(rgb.c, 1.0f / 2.2f);

        uint8_t r = static_cast<uint8_t>(255.0 * rgb.a);
        uint8_t g = static_cast<uint8_t>(255.0 * rgb.b);
        uint8_t b = static_cast<uint8_t>(255.0 * rgb.c);
        HPEN pen = CreatePen(PS_SOLID, 5, RGB(r, g, b));
        POINT pt = {};

        HGDIOBJ currentPen = SelectObject(dc, pen);
        MoveToEx(dc, i, 0, &pt);
        LineTo(dc, i, _height);

        SelectObject(dc, currentPen);
        DeleteObject(pen);
    }
}
