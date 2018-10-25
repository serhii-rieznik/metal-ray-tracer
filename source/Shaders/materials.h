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
    SampledMaterialProperties result = {};

    wI = -wI;
    float3 m = normalize(wI + wO);

    float NdotI = dot(n, wI);
    float MdotO = dot(m, wO);
    float NdotM = dot(n, m);
    float NdotO = saturate(dot(n, wO));

    if (NdotO <= 0.0f)
        return result;

    switch (material.type)
    {
        case MATERIAL_CONDUCTOR:
        {
            float3 F = material.specular * fresnelConductor(MdotO);
            float D = ggxNormalDistribution(material.roughness * material.roughness, NdotM);
            float G = ggxVisibilityTerm(material.roughness * material.roughness, NdotO, NdotI);
            result.bsdf0 = F * (D * G / (4.0 * NdotI));
            result.pdf0 = D * NdotM / (4.0 * MdotO);
            break;
        }

        case MATERIAL_PLASTIC:
        {
            float3 F = material.specular * fresnelDielectric(MdotO);
            float D = ggxNormalDistribution(material.roughness * material.roughness, NdotM);
            float G = ggxVisibilityTerm(material.roughness * material.roughness, NdotO, NdotI);
            result.bsdf0 = F * (D * G / (4.0 * NdotI));
            result.pdf0 = D * NdotM / (4.0 * MdotO);

            result.bsdf1 = material.diffuse * INVERSE_PI * NdotO;
            result.pdf1 = INVERSE_PI * NdotO;
            break;
        }

        case MATERIAL_DIELECTRIC:
        {
            break;
        }

        default:
        {
            result.bsdf1 = material.diffuse * INVERSE_PI * NdotO;
            result.pdf1 = INVERSE_PI * NdotO;
            break;
        }
    }

    return result;
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
            float3 m = sampleGGXDistribution(n, randomSample.wz, material.roughness);
            float MdotI = max(0.0, -dot(m, wI));
            float F = fresnelDielectric(MdotI);

            if (randomSample.y > F)
            {
                wO = sampleCosineWeightedHemisphere(n, randomSample.wx);
                pdf = INVERSE_PI * dot(n, wO);
                weight = material.diffuse;
            }
            else
            {
                wO = reflect(wI, m);
                float NdotO = dot(n, wO);
                float NdotI = -dot(n, wI);
                if (NdotO > 0.0)
                {
                    float MdotO = dot(m, wO);
                    float NdotM = dot(n, m);
                    float D = ggxNormalDistribution(material.roughness * material.roughness, NdotM);
                    float G = ggxVisibilityTerm(material.roughness * material.roughness, NdotO, NdotI);
                    pdf = D * NdotM / (4.0 * MdotO);
                    weight = material.specular * ((G * MdotO) / (NdotI * NdotM));
                }
            }
            break;
        }

        case MATERIAL_DIELECTRIC:
        {
            float etaAir = 1.0f;
            float etaMaterial = 1.5f;
            float eta = 0.0f;

            float NdotI = dot(n, wI);

            wI = -wI;

            if (NdotI < 0.0) // entering material
            {
                NdotI = -NdotI;
                eta = etaAir / etaMaterial;
            }
            else
            {
                n = -n;
                eta = etaMaterial / etaAir;
            }

            float3 m = sampleGGXDistribution(n, randomSample.wz, material.roughness);
            float MdotI = dot(m, wI);
            float cosThetaI = MdotI;
            float sinThetaTSquared = eta * eta * max(0.0f, 1.0f - cosThetaI * cosThetaI);
            float F = (sinThetaTSquared < 1.0) ? fresnelDielectric(MdotI) : 1.0;

            if (randomSample.y > F)
            {
                float cosThetaT = sqrt(1.0f - sinThetaTSquared);
                wO = (eta * cosThetaI - cosThetaT) * m - eta * wI;
                pdf = 1.0f;
                weight = 1.0f;
            }
            else
            {
                wO = reflect(-wI, m);
                float NdotO = dot(n, wO);
                if (NdotO > 0.0)
                {
                    float MdotO = dot(m, wO);
                    float NdotM = dot(n, m);
                    float D = ggxNormalDistribution(material.roughness * material.roughness, NdotM);
                    float G = ggxVisibilityTerm(material.roughness * material.roughness, NdotO, NdotI);
                    pdf = D * NdotM / (4.0 * MdotO);
                    weight = material.specular * ((G * MdotO) / (NdotI * NdotM));
                }
            }
            break;
        }

        default:
        {
            wO = sampleCosineWeightedHemisphere(n, randomSample.wx);
            pdf = INVERSE_PI * dot(n, wO);
            weight = material.diffuse;
            break;
        }
    }

    return { wO, pdf, weight };
}
