//
//  materials.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "structures.h"

namespace diffuse
{

inline SampledMaterial evaluate(device const Material& material, float3 nO, float3 wI, float3 wO, float wavelength)
{
    SampledMaterial result = { };
    result.direction = wO;
    float NdotO = dot(nO, wO);
    float NdotI = -dot(nO, wI);
    result.valid = uint(NdotO * NdotI > 0.0f) * (NdotO > 0.0f) * (NdotI > 0.0f);
    if (result.valid)
    {
        float spectrumSample = GPUSpectrumSample(material.diffuse, wavelength);
        result.normal = nO;
        result.bsdf = spectrumSample * (INVERSE_PI * NdotO);
        result.pdf = INVERSE_PI * NdotO;
        result.weight = spectrumSample;
    }
    return result;
}

inline float3 sample(device const Material& material, float3 nO, float3 wI,
    device const RandomSample& randomSample, float wavelength)
{
    return sampleCosineWeightedHemisphere(nO, randomSample.bsdfSample);
}

}
