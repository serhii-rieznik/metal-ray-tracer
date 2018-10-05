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

    switch (material.type)
    {
        case MATERIAL_MIRROR:
        {
            float3 r = reflect(wI, n);
            float isMirrorDirection = float(abs(1.0 - dot(r, wO)) < ANGLE_EPSILON);
            result.bsdf = isMirrorDirection;
            result.pdf = isMirrorDirection;
            result.bsdf_over_pdf = isMirrorDirection;
            break;
        }

        case MATERIAL_CONDUCTOR:
        {
            wI = -wI;
            float3 h = normalize(wI + wO);
            
            float NdotI = saturate(dot(n, wI));
            float NdotO = saturate(dot(n, wO));
            float HdotO = saturate(dot(h, wO));
            float NdotH = max(dot(n, h), ANGLE_EPSILON);

            float D = ggxNormalDistribution(material.roughness * material.roughness, NdotH);
            float F = fresnelConductor(HdotO);
            float G = ggxVisibilityTerm(material.roughness * material.roughness, NdotI, NdotO);

            result.bsdf = D * F * G / (4.0 * NdotI); // D * F * G;
            result.pdf = D * NdotH;
            result.bsdf_over_pdf = F * G / (4.0 * NdotI * NdotH + ANGLE_EPSILON);
            break;
        }

        default:
        {
            float NdotO = saturate(dot(n, wO));
            result.bsdf = INVERSE_PI * NdotO;
            result.pdf = INVERSE_PI * NdotO;
            result.bsdf_over_pdf = 1.0f;
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
