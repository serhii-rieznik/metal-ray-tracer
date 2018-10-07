//
//  materials.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "structures.h"

SampledMaterial sampleMaterial(device const Material& material, float3 n, float3 wI, float3 wO)
{
    SampledMaterial result = { wO };

    float NdotO = saturate(dot(n, wO));
    if (NdotO <= 0.0f)
        return result;

    switch (material.type)
    {
        case MATERIAL_MIRROR:
        {
            float3 r = reflect(wI, n);
            float isMirrorDirection = float(abs(1.0 - dot(r, wO)) < ANGLE_EPSILON);
            result.bsdf = isMirrorDirection;
            result.pdf = isMirrorDirection;
            result.weight = isMirrorDirection;
            break;
        }

        case MATERIAL_CONDUCTOR:
        {
            wI = -wI;
            float3 h = normalize(wI + wO);
            
            float NdotI = dot(n, wI);
            float HdotO = dot(h, wO);
            float NdotH = dot(n, h);
            {
                float F = fresnelConductor(HdotO);
                float D = ggxNormalDistribution(material.roughness * material.roughness, NdotH);
                float G = ggxVisibilityTerm(material.roughness * material.roughness, NdotO, NdotI);
                result.bsdf = F * D * G / (4.0 * NdotI);
                result.pdf = D * NdotH / (4.0 * HdotO);
                result.weight = F * ((G * HdotO) / (NdotI * NdotH));
            }
            break;
        }

        default:
        {
            result.bsdf = INVERSE_PI * NdotO;
            result.pdf = INVERSE_PI * NdotO;
            result.weight = 1.0f;
            break;
        }
    }
    return result;
}

SampledMaterial sampleMaterial(device const Material& material, float3 n, float3 wI, device const float4& noiseSample)
{
    float3 reflected = reflect(wI, n);

    float3 wO;
    switch (material.type)
    {
        case MATERIAL_MIRROR:
        {
            wO = reflect(wI, n);
            break;
        }

        case MATERIAL_CONDUCTOR:
        {
            wO = sampleGGXDistribution(reflected, noiseSample.wz, material.roughness);
            break;
        }

        default:
        {
            wO = sampleCosineWeightedHemisphere(n, noiseSample.wx);
        }
    }

    return sampleMaterial(material, n, wI, wO);
}
