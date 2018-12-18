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

inline SampledMaterial evaluate(device const Material& material, float3 nO, float3 wI, float3 wO)
{
    SampledMaterial result = { };
    result.direction = wO;
    float NdotO = dot(nO, wO);
    float NdotI = -dot(nO, wI);
    result.valid = uint(NdotO * NdotI > 0.0f) * (NdotO > 0.0f) * (NdotI > 0.0f);
    if (result.valid)
    {
        result.bsdf = material.diffuse * (INVERSE_PI * NdotO);
        result.pdf = INVERSE_PI * NdotO;
        result.weight = material.diffuse;
    }
    return result;
}

inline SampledMaterial sample(device const Material& material, float3 nO, float3 wI, device const RandomSample& randomSample)
{
    float3 wO = sampleCosineWeightedHemisphere(nO, randomSample.bsdfSample);
    return evaluate(material, nO, wI, wO);
}

}
