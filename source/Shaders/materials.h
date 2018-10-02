//
//  materials.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "structures.h"

SampledMaterial sampleMaterial(device const Material& material, float3 n, float3 wI, device const float4& noiseSample)
{
    SampledMaterial result = {};
    result.direction = sampleCosineWeightedHemisphere(n, noiseSample.wx);
    result.throughputScale = material.diffuse;
    return result;
}

SampledMaterial sampleMaterial(device const Material& material, float3 n, float3 wI, float3 wO)
{
    float cosTheta = max(0.0, dot(n, wO));

    SampledMaterial result = {};
    result.direction = wO;
    result.bsdf = INVERSE_PI * cosTheta;
    result.pdf = INVERSE_PI * cosTheta;
    result.throughputScale = result.bsdf * material.diffuse;
    return result;
}
