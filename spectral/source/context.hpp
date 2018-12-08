#pragma once

#include "../stdafx.h"
#include "spectrum.hpp"

class Context
{
public:
    Context();
    ~Context();

    void resize(uint32_t w, uint32_t h);
    void draw(HDC dc);

private:
    uint32_t _width = 0;
    uint32_t _height = 0;
    SampledSpectrum _spectrumX;
    SampledSpectrum _spectrumY;
    SampledSpectrum _spectrumZ;
};

// 10cm = 14knots
