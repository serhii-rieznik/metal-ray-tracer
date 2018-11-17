//
//  materials.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "structures.h"

inline SampledMaterial evaluateMicrofacetTransmission(device const Material& material, float3 nO, float3 wI, float3 wO, bool pdfIncludesF)
{
    bool exitingMaterial = (dot(nO, wI) > 0.0f);

    float nS = exitingMaterial ? -1.0f : 1.0f;
    float etaI = exitingMaterial ? material.intIOR : material.extIOR;
    float etaO = exitingMaterial ? material.extIOR : material.intIOR;
    float3 n = nS * nO;

    float3 m = nS * normalize(wI * etaI - wO * etaO);

    float NdotI = -dot(n, wI);
    float NdotO = dot(n, wO);
    float NdotM = abs(dot(n, m));
    float MdotI = abs(dot(m, wI));
    float MdotO = abs(dot(m, wO));

    float eta = etaI / etaO;
    float alpha = remapRoughness(material.roughness, NdotI);
    float F = fresnelDielectric(wI, m, etaI, etaO);
    float D = ggxNormalDistribution(alpha, n, m);
    float G = ggxVisibilityTerm(alpha, wI, wO, n, m);
    float J = sqr(eta) * MdotO / sqr(MdotI + eta * MdotO);

    SampledMaterial result = {};
    {
        result.valid = uint(NdotO * NdotI < 0.0f);

        if (result.valid)
        {
            result.bsdf = material.transmittance * sqr(eta) * (1.0f - F) * D * G * MdotI * MdotO / (NdotI * sqr(MdotI + eta * MdotO));
            result.pdf = D * NdotM * J;
            result.weight = result.bsdf / result.pdf; // material.transmittance * ((1.0f - F) * G * MdotI / (NdotI * NdotM));
        }

        if (pdfIncludesF)
        {
            result.pdf *= (1.0f - F);
            result.weight /= (1.0f - F);
        }
    }
    return result;
}

inline SampledMaterial evaluateMicrofacetReflection(device const Material& material, float3 nO, float3 wI, float3 wO, bool pdfIncludesF)
{
    bool exitingMaterial = (dot(nO, wI) > 0.0f);
    float nS = exitingMaterial ? -1.0f : 1.0f;
    float etaI = exitingMaterial ? material.intIOR : material.extIOR;
    float etaO = exitingMaterial ? material.extIOR : material.intIOR;
    float3 n = nS * nO;
    float3 m = normalize(wO - wI);

    float NdotI = -dot(n, wI);
    float NdotO = dot(n, wO);
    float MdotO = dot(m, wO);
    float NdotM = dot(n, m);

    float alpha = remapRoughness(material.roughness, NdotI);
    float F = fresnelDielectric(wI, m, etaI, etaO);
    float D = ggxNormalDistribution(alpha, n, m);
    float G = ggxVisibilityTerm(alpha, wI, wO, n, m);
    float J = 1.0f / (4.0 * MdotO);

    SampledMaterial result;
    {
        result.valid = uint(NdotO * NdotI > 0.0f) * (NdotO > 0.0f) * (NdotI > 0.0f);
        if (result.valid)
        {
            result.bsdf = material.specular * abs(F * D * G / (4.0 * NdotI));
            result.pdf = abs(D * NdotM * J);
            result.weight = material.specular * abs(F * G * MdotO / (NdotM * NdotI));
        }
        if (pdfIncludesF)
        {
            result.pdf *= F;
            result.weight /= F;
        }
    }
    return result;
}

inline SampledMaterial evaluateMaterial(device const Material& material, float3 nO, float3 wI, float3 wO,
    device const RandomSample& randomSample)
{
    SampledMaterial result = { wO };

    switch (material.type)
    {
        case MATERIAL_DIFFUSE:
        {
            float3 n = nO;
            float NdotO = dot(n, wO);
            float NdotI = -dot(n, wI);
            result.bsdf = material.diffuse * INVERSE_PI * NdotO;
            result.pdf = INVERSE_PI * NdotO;
            result.valid = uint(NdotO > 0.0f) * uint(NdotI > 0.0f);
            break;
        }

        case MATERIAL_CONDUCTOR:
        {
            float3 n = nO;
            float NdotO = dot(n, wO);
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);
            float3 m = normalize(wO - wI);
            float NdotM = dot(n, m);
            float MdotO = dot(m, wO);

            float F = FresnelConductor(wI, m, material.extIOR, material.intIOR);
            float D = ggxNormalDistribution(alpha, n, m);
            float G = ggxVisibilityTerm(alpha, wI, wO, n, m);
            float J = 1.0f / (4.0 * MdotO);
            result.bsdf = material.specular * (F * D * G / (4.0 * NdotI));
            result.pdf = D * NdotM * J;
            result.valid = uint(NdotO > 0.0f) * uint(NdotI > 0.0f) * uint(result.pdf > 0.0f);
            break;
        }

        case MATERIAL_PLASTIC:
        {
            float3 n = nO;
            float3 m = normalize(wO - wI);
            
            float NdotI = -dot(n, wI);
            float NdotO = dot(n, wO);
            float NdotM = dot(n, m);
            float MdotO = dot(m, wO);

            float F = fresnelDielectric(wI, m, material.extIOR, material.intIOR);
            float alpha = remapRoughness(material.roughness, NdotI);
            float D = ggxNormalDistribution(alpha, n, m);
            float G = ggxVisibilityTerm(alpha, wI, wO, n, m);
            float J = 1.0f / (4.0 * MdotO);

            result.bsdf = material.diffuse * INVERSE_PI * NdotO * (1.0f - F) +
                material.specular * (F * D * G / (4.0 * NdotI));

            result.pdf = INVERSE_PI * NdotO * (1.0f - F) + D * NdotM * J * F;
            
            result.valid = uint(NdotO > 0.0f) * uint(NdotI > 0.0f) * uint(NdotM > 0.0f);
            break;
        }

        case MATERIAL_DIELECTRIC:
        {
            bool exitingMaterial = (dot(nO, wI) > 0.0f);
            float nS = exitingMaterial ? -1.0f : 1.0f;

            float3 n = nS * nO;

            float NdotO = dot(n, wO);
            float NdotI = -dot(n, wI);

            if (NdotO * NdotI < 0.0f)
            {
                result = evaluateMicrofacetTransmission(material, nO, wI, wO, false);
            }
            else
            {
                result = evaluateMicrofacetReflection(material, nO, wI, wO, false);
            }
            break;
        }

        default:
        {
            break;
        }
    }

    result.valid *= uint(dot(result.bsdf, result.bsdf) > 0.0f) * uint(result.pdf > 0.0f);
    return result;
}

inline SampledMaterial sampleMaterial(device const Material& material, float3 nO, float3 wI, device const RandomSample& randomSample)
{
    float etaI = material.extIOR;
    float etaO = material.intIOR;

    float3 wO = {};

    SampledMaterial result = { };

    switch (material.type)
    {
        case MATERIAL_DIFFUSE:
        {
            float3 n = nO;
            float NdotI = -dot(n, wI);
            
            wO = sampleCosineWeightedHemisphere(n, randomSample.bsdfSample);
            float NdotO = dot(n, wO);

            result.bsdf = material.diffuse * INVERSE_PI * NdotO;
            result.pdf = INVERSE_PI * NdotO;
            result.weight = material.diffuse;
            result.valid = uint(NdotI > 0.0f) * uint(NdotO > 0.0f);
            break;
        }

        case MATERIAL_CONDUCTOR:
        {
            float3 n = nO;
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);
            float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, alpha);
            float NdotM = dot(n, m);

            wO = reflect(wI, m);
            float NdotO = dot(n, wO);

            float MdotO = dot(m, wO);
            float F = FresnelConductor(wI, m, material.extIOR, material.intIOR);
            float D = ggxNormalDistribution(alpha, n, m);
            float G = ggxVisibilityTerm(alpha, wI, wO, n, m);
            float J = 1.0f / (4.0 * MdotO);

            result.bsdf = material.specular * (F * D * G / (4.0 * NdotI));
            result.pdf = D * NdotM * J;
            result.weight = material.specular * (F * G * MdotO / (NdotM * NdotI));
            result.valid = uint(NdotI > 0.0f) * uint(NdotO > 0.0f) * uint(NdotM > 0.0f);
            break;
        }

        case MATERIAL_PLASTIC:
        {
            float3 n = nO;
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);
            float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, alpha);

            float F = fresnelDielectric(wI, m, etaI, etaO);
            //*
            if (randomSample.componentSample >= F)
            {
                wO = sampleCosineWeightedHemisphere(n, randomSample.bsdfSample);
                //*
                float NdotO = dot(n, wO);
                result.bsdf = material.diffuse * INVERSE_PI * NdotO * (1.0f - F);
                result.pdf = INVERSE_PI * NdotO * (1.0 - F);
                result.weight = material.diffuse;
                result.valid = uint(NdotO > 0.0f);
                // */
            }
            else
            {
                wO = reflect(wI, m);
                result = evaluateMicrofacetReflection(material, nO, wI, wO, true);
            }
            break;
        }

        case MATERIAL_DIELECTRIC:
        {
            float3 n = nO;
            if (dot(nO, wI) > 0.0f)
            {
                n = -n;
                float etaX = etaI;
                etaI = etaO;
                etaO = etaX;
            }
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);
            float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, alpha);

            float F = fresnelDielectric(wI, m, etaI, etaO);
            if (randomSample.componentSample >= F)
            {
                float cosThetaI = -dot(m, wI);
                float sinThetaTSquared = sqr(etaI / etaO) * (1.0 - cosThetaI * cosThetaI);
                float cosThetaT = sqrt(saturate(1.0 - sinThetaTSquared));
                wO = wI * (etaI / etaO) + m * (etaI / etaO * cosThetaI - cosThetaT);
                result = evaluateMicrofacetTransmission(material, nO, wI, wO, true);
            }
            else
            {
                wO = reflect(wI, m);
                result = evaluateMicrofacetReflection(material, nO, wI, wO, true);
            }
            break;
        }

        default:
        {
            break;
        }
    }

    result.direction = wO;
    // result.valid *= uint(dot(result.bsdf, result.bsdf) > 0.0f) * uint(result.pdf > 0.0f) * uint(!isinf(result.pdf));

    return result;
}
