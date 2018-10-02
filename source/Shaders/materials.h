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
    switch (material.type)
    {
        case MATERIAL_MIRROR:
        {
            result.direction = reflect(wI, n);
            result.bsdf = 1.0f;
            result.pdf = 1.0f;
            result.throughputScale = 1.0f;
            break;
        }

        default:
        {
            float3 wO = sampleCosineWeightedHemisphere(n, noiseSample.wx);
            float cosTheta = dot(n, wO);
            result.direction = wO;
            result.bsdf = INVERSE_PI * cosTheta;
            result.pdf = INVERSE_PI * cosTheta;
            result.throughputScale = material.diffuse;
        }
    }
    return result;
}

SampledMaterial sampleMaterial(device const Material& material, float3 n, float3 wI, float3 wO)
{
    float cosTheta = max(0.0, dot(n, wO));

    SampledMaterial result = {};
    result.direction = wO;

    switch (material.type)
    {
        case MATERIAL_MIRROR:
        {
            float3 r = reflect(wI, n);
            bool mirrorDirection = abs(1.0 - dot(r, wO)) < ANGLE_EPSILON;
            result.bsdf = mirrorDirection ? cosTheta : 0.0f;
            result.pdf = mirrorDirection ? 1.0f : 0.0f;
            result.throughputScale = mirrorDirection ? 1.0f : 0.0f;
            break;
        }

        default:
        {
            result.bsdf = INVERSE_PI * cosTheta;
            result.pdf = INVERSE_PI * cosTheta;
            result.throughputScale = result.bsdf * material.diffuse;
        }
    }
    return result;
}
