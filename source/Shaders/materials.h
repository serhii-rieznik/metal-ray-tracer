//
//  materials.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "structures.h"

SampledMaterialProperties sampleMaterialProperties(device const Material& material, float3 n, float3 wI, float3 wO)
{
    float3 bsdf = 0.0;
    float pdf = 0.0;

    float NdotO = saturate(dot(n, wO));
    if (NdotO <= 0.0f)
        return {};

    switch (material.type)
    {
        case MATERIAL_CONDUCTOR:
        {
            wI = -wI;
            float3 m = normalize(wI + wO);
            
            float NdotI = dot(n, wI);
            float MdotO = dot(m, wO);
            float NdotM = dot(n, m);
            {
                float3 F = material.specular * fresnelConductor(MdotO);
                float D = ggxNormalDistribution(material.roughness * material.roughness, NdotM);
                float G = ggxVisibilityTerm(material.roughness * material.roughness, NdotO, NdotI);
                bsdf = F * (D * G / (4.0 * NdotI));
                pdf = D * NdotM / (4.0 * MdotO);
            }
            break;
        }

        case MATERIAL_PLASTIC:
        {
            bsdf = 0.0;
            pdf = 1.0;
            break;
        }

        case MATERIAL_DIELECTRIC:
        {
            bsdf = 0.0;
            pdf = 1.0;
            break;
        }

        default:
        {
            bsdf = material.diffuse * INVERSE_PI * NdotO;
            pdf = INVERSE_PI * NdotO;
            break;
        }
    }

    return { bsdf, pdf };
}

SampledMaterial sampleMaterial(device const Material& material, float3 n, float3 wI, device const float4& randomSample)
{
    float3 wO;
    float pdf = 0.0f;
    float3 weight = 0.0f;
    switch (material.type)
    {
        case MATERIAL_CONDUCTOR:
        {
            float3 m = sampleGGXDistribution(n, randomSample.wz, material.roughness);
            wO = reflect(wI, m);
            float NdotO = dot(n, wO);
            if (NdotO > 0.0)
            {
                float NdotI = -dot(n, wI);
                float MdotO = dot(m, wO);
                float NdotM = dot(n, m);
                float3 F = material.specular * fresnelConductor(MdotO);
                float D = ggxNormalDistribution(material.roughness * material.roughness, NdotM);
                float G = ggxVisibilityTerm(material.roughness * material.roughness, NdotO, NdotI);
                pdf = D * NdotM / (4.0 * MdotO);
                weight = F * ((G * MdotO) / (NdotI * NdotM));
            }
            break;
        }

        case MATERIAL_PLASTIC:
        {
            wO = sampleCosineWeightedHemisphere(n, randomSample.wx);
            weight = 0.0f;
            pdf = 1.0f;
            break;
        }

        case MATERIAL_DIELECTRIC:
        {
            wO = sampleCosineWeightedHemisphere(n, randomSample.wx);
            weight = 0.0f;
            pdf = 1.0f;
            break;
        }

        default:
        {
            wO = sampleCosineWeightedHemisphere(n, randomSample.wx);
            pdf = INVERSE_PI * dot(n, wO);
            weight = material.diffuse;
        }
    }

    return { wO, pdf, weight };
}
