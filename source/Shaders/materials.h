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
            evaluateDiffuseReflection(material, n, normalize(wO - wI), wI, wO, result.bsdf, result.pdf, result.weight);
            break;
        }

        case MATERIAL_CONDUCTOR:
        {
            float3 m = normalize(wO - wI);
            FresnelSample F = fresnelConductor(wI, m, material.extIOR, material.intIOR);
            evaluateMicrofacetReflection(material, n, normalize(wO - wI), wI, wO, F, result.bsdf, result.pdf, result.weight);
            break;
        }

        case MATERIAL_PLASTIC:
        {
            float3 m = normalize(wO - wI);
            FresnelSample F = fresnelDielectric(wI, m, material.extIOR, material.intIOR);

            packed_float3 diffuseBsdf = 0.0f;
            float diffusePdf = 0.0f;
            packed_float3 diffuseWeight = 0.0f;
            bool hasDiffuse = evaluateDiffuseReflection(material, n, m, wI, wO, diffuseBsdf, diffusePdf, diffuseWeight);

            packed_float3 specularBsdf = 0.0f;
            float specularPdf = 0.0f;
            packed_float3 specularWeight = 0.0f;
            bool hasSpecular = evaluateMicrofacetReflection(material, n, m, wI, wO, F, specularBsdf, specularPdf, specularWeight);

            if (hasDiffuse || hasSpecular)
            {
                result.bsdf = diffuseBsdf * (1.0f - F.value) + specularBsdf;
                result.pdf = diffusePdf * (1.0f - F.value) + specularPdf * F.value;
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
            float alpha = remapRoughness(material.roughness, NdotI);
            float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, alpha);
            FresnelSample F = fresnelDielectric(wI, m, etaI, etaO);

            if (evaluateMicrofacetTransmission(material, n, m, wI, wO, F, result.bsdf, result.pdf, result.weight))
            {
                result.pdf *= 1.0f - F.value;
                result.weight /= 1.0f - F.value;
            }
            else if (evaluateMicrofacetReflection(material, n, m, wI, wO, F, result.bsdf, result.pdf, result.weight))
            {
                result.pdf *= F.value;
                result.weight /= F.value;
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
            FresnelSample F = fresnelDielectric(wI, m, etaI, etaO);
            if (randomSample.componentSample >= F.value)
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
            FresnelSample F = fresnelDielectric(wI, m, etaI, etaO);
            if (randomSample.componentSample >= F.value)
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
