//
//  materials.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "structures.h"

inline SampledMaterial evaluateMaterial(device const Material& material, float3 n, float3 wI, float3 wO,
    device const RandomSample& randomSample)
{
    float etaI = material.extIOR;
    float etaO = material.intIOR;

    SampledMaterial result = { wO };

    switch (material.type)
    {
        case MATERIAL_DIFFUSE:
        {
            float NdotO = dot(n, wO);
            if (NdotO > 0.0f)
            {
                result.bsdf = material.diffuse * INVERSE_PI * NdotO;
                result.pdf = INVERSE_PI * NdotO;
                result.weight = material.diffuse;
            }
            break;
        }

        case MATERIAL_CONDUCTOR:
        {
            float NdotO = dot(n, wO);
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);
            if ((NdotO > 0.0f) && (NdotI > 0.0f))
            {
                float3 m = normalize(wO - wI);
                float NdotM = dot(n, m);
                float MdotO = dot(m, wO);
                float F = fresnelConductor(MdotO);
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                float J = 1.0f / (4.0 * MdotO);
                result.bsdf = material.specular * F * D * G / (4.0 * NdotI);
                result.pdf = D * NdotM * J;
                result.weight = material.specular * F * G * MdotO / (NdotI * NdotM);
            }
            break;
        }

        case MATERIAL_PLASTIC:
        {
            float3 m = normalize(wO - wI);
            float NdotM = dot(n, m);
            float NdotO = dot(n, wO);
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);
            if ((NdotO > 0.0f) && (NdotI > 0.0f))
            {
                float MdotO = dot(m, wO);
                float F = fresnelDielectric(wI, m, etaI, etaO);
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                float J = 1.0f / (4.0 * MdotO);

                result.bsdf = mix(material.diffuse * INVERSE_PI * NdotO, material.specular * D * G / (4.0 * NdotI), F);
                result.pdf = mix(INVERSE_PI * NdotO, D * NdotM * J, F);
                result.weight = result.bsdf / result.pdf;
            }
            break;
        }

        case MATERIAL_DIELECTRIC:
        {
            if (dot(n, wI) > 0.0f)
            {
                n = -n;
                float etaX = etaI;
                etaI = etaO;
                etaO = etaX;
            }
            
            float NdotI = -dot(n, wI);
            float NdotO = dot(n, wO);
            float alpha = remapRoughness(material.roughness, NdotI);

            float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, alpha);
            float NdotM = abs(dot(n, m));

            float F = fresnelDielectric(wI, m, etaI, etaO);

            if ((NdotI * NdotO < 0.0f) && (F < 1.0f)) // ray is refracted
            {
                float MdotI = abs(dot(m, wI));
                float MdotO = abs(dot(m, wO));
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                float J = MdotO / sqr(etaI * MdotI + etaO * MdotO);

                result.bsdf = material.transmittance * (1.0f - F) * D * G * MdotI * MdotO /
                    (NdotI * sqr(etaI * MdotI + etaO * MdotO));
                result.pdf = (1.0f - F) * D * NdotM * J;
                result.weight = material.transmittance * G * MdotI / (NdotI * NdotM);
            }
            else
            {
                float MdotO = abs(dot(m, wO));
                float F = fresnelDielectric(wI, m, etaI, etaO);
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                float J = 1.0f / (4.0 * MdotO);
                result.bsdf = material.specular * F * D * G / (4.0 * NdotI);
                result.pdf = F * D * NdotM * J;
                result.weight = material.specular * G * MdotO / (NdotI * NdotM);
            }
            break;
        }

        default:
        {
            break;
        }
    }

    // result.weight = (result.pdf != 0.0f) ? (result.bsdf / result.pdf) : 0.0f;

    return result;
}

inline SampledMaterial sampleMaterial(device const Material& material, float3 n, float3 wI, device const RandomSample& randomSample)
{
    float etaI = material.extIOR;
    float etaO = material.intIOR;

    float3 wO = {};

    switch (material.type)
    {
        case MATERIAL_DIFFUSE:
        {
            wO = sampleCosineWeightedHemisphere(n, randomSample.bsdfSample);
            break;
        }

        case MATERIAL_CONDUCTOR:
        {
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);
            float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, alpha);
            wO = reflect(wI, m);
            break;
        }

        case MATERIAL_PLASTIC:
        {
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);
            float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, alpha);
            float pdfSpecular = fresnelDielectric(wI, m, etaI, etaO);
            if (randomSample.componentSample >= pdfSpecular)
            {
                wO = sampleCosineWeightedHemisphere(n, randomSample.bsdfSample);
            }
            else
            {
                wO = reflect(wI, m);
            }
            break;
        }

        case MATERIAL_DIELECTRIC:
        {
            if (dot(n, wI) > 0.0f)
            {
                n = -n;
                float etaX = etaI;
                etaI = etaO;
                etaO = etaX;
            }
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);
            float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, alpha);
            float pdfSpecular = fresnelDielectric(wI, m, etaI, etaO);
            if (randomSample.componentSample >= pdfSpecular)
            {
                float cosThetaI = -dot(m, wI);
                float sinThetaTSquared = sqr(etaI / etaO) * (1.0 - cosThetaI * cosThetaI);
                float cosThetaT = sqrt(saturate(1.0 - sinThetaTSquared));
                wO = wI * (etaI / etaO) + m * (etaI / etaO * cosThetaI - cosThetaT);
            }
            else
            {
                wO = reflect(wI, m);
            }
            break;
        }

        default:
        {
            break;
        }
    }

    SampledMaterial result = evaluateMaterial(material, n, wI, wO, randomSample);

    return result;
}
