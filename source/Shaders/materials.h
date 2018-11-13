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
            float NdotI = -dot(n, wI);
            result.bsdf = material.diffuse * INVERSE_PI * NdotO;
            result.pdf = INVERSE_PI * NdotO;
            result.valid = uint(NdotO > 0.0f) * uint(NdotI > 0.0f);
            break;
        }

        case MATERIAL_CONDUCTOR:
        {
            float NdotO = dot(n, wO);
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);
            float3 m = normalize(wO - wI);
            float NdotM = dot(n, m);
            float MdotO = dot(m, wO);

            float F = FresnelConductor(wI, m, material.extIOR, material.intIOR);
            float D = ggxNormalDistribution(alpha, NdotM);
            float G = ggxVisibilityTerm(alpha, wI, wO, n);
            float J = 1.0f / (4.0 * MdotO);
            result.bsdf = material.specular * (F * D * G / (4.0 * NdotI));
            result.pdf = D * NdotM * J;
            result.valid = uint(NdotO > 0.0f) * uint(NdotI > 0.0f) * uint(NdotM > 0.0f);
            break;
        }

        case MATERIAL_PLASTIC:
        {
            float NdotO = dot(n, wO);
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);
            float3 m = normalize(wO - wI);
            float NdotM = dot(n, m);
            float MdotO = dot(m, wO);

            float F = fresnelDielectric(wI, m, material.extIOR, material.intIOR);
            float D = ggxNormalDistribution(alpha, NdotM);
            float G = ggxVisibilityTerm(alpha, wI, wO, n);
            float J = 1.0f / (4.0 * MdotO);

            result.bsdf = material.diffuse * INVERSE_PI * NdotO * (1.0f - F) +
                material.specular * (F * D * G / (4.0 * NdotI));

            result.pdf = INVERSE_PI * NdotO * (1.0 - F) +
                D * NdotM * J * F;

            result.valid = uint(NdotO > 0.0f) * uint(NdotI > 0.0f) * uint(NdotM > 0.0f);
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
            
            float NdotO = dot(n, wO);
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);

            if (NdotI * NdotO > 0.0f)
            {
                float3 h = normalize(wO - wI);
                float NdotH = dot(n, h);
                float HdotO = dot(h, wO);
                float F = fresnelDielectric(wI, h, etaI, etaO);
                float D = ggxNormalDistribution(alpha, NdotH);
                float G = ggxVisibilityTerm(alpha, wI, wO, h);
                float J = 1.0f / (4.0 * HdotO);
                result.bsdf = material.specular * (F * D * G / (4.0 * NdotI));
                result.pdf = D * NdotH * J * F;
            }

            if (NdotI * NdotO < 0.0f)
            {
                float3 h = normalize(wO * etaO - wI * etaI);
                float NdotH = dot(n, h);
                float HdotI = dot(h, wI);
                float HdotO = dot(h, wO);
                float F = fresnelDielectric(wI, h, etaI, etaO);
                float D = ggxNormalDistribution(alpha, NdotH);
                float G = ggxVisibilityTerm(alpha, wI, wO, h);
                float J = HdotO / sqr(etaI * HdotI + etaO * HdotO);

                result.bsdf += material.transmittance *
                    abs((1.0 - F) * D * G * HdotI * HdotO / (NdotI * sqr(etaI * HdotI + etaO * HdotO)));

                result.pdf += D * NdotH * J * (1.0f - F);
            }

            result.valid = uint(result.pdf > 0.0f);

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

inline SampledMaterial sampleMaterial(device const Material& material, float3 n, float3 wI, device const RandomSample& randomSample)
{
    float etaI = material.extIOR;
    float etaO = material.intIOR;

    float3 wO = {};

    SampledMaterial result = { };

    switch (material.type)
    {
        case MATERIAL_DIFFUSE:
        {
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
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);
            float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, alpha);
            float NdotM = dot(n, m);

            wO = reflect(wI, m);
            float NdotO = dot(n, wO);

            float MdotO = dot(m, wO);
            float F = FresnelConductor(wI, m, material.extIOR, material.intIOR);
            float D = ggxNormalDistribution(alpha, NdotM);
            float G = ggxVisibilityTerm(alpha, wI, wO, n);
            float J = 1.0f / (4.0 * MdotO);

            result.bsdf = material.specular * (F * D * G / (4.0 * NdotI));
            result.pdf = D * NdotM * J;
            result.weight = material.specular * (F * G * MdotO / (NdotM * NdotI));
            result.valid = uint(NdotI > 0.0f) * uint(NdotO > 0.0f) * uint(NdotM > 0.0f);
            break;
        }

        case MATERIAL_PLASTIC:
        {
            float NdotI = -dot(n, wI);
            float alpha = remapRoughness(material.roughness, NdotI);
            float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, alpha);
            float NdotM = dot(n, m);

            float F = fresnelDielectric(wI, m, etaI, etaO);

            if (randomSample.componentSample >= F)
            {
                wO = sampleCosineWeightedHemisphere(n, randomSample.bsdfSample);
                float NdotO = dot(n, wO);

                result.bsdf = material.diffuse * INVERSE_PI * NdotO * (1.0f - F);
                result.pdf = INVERSE_PI * NdotO * (1.0 - F);
                result.weight = material.diffuse;
                result.valid = uint(NdotO > 0.0f);
            }
            else
            {
                wO = reflect(wI, m);
                float NdotO = dot(n, wO);

                float MdotO = dot(m, wO);
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                float J = 1.0f / (4.0 * MdotO);

                result.bsdf = material.specular * (F * D * G / (4.0 * NdotI));
                result.pdf = D * NdotM * J * F;
                result.weight = material.specular * (G * MdotO / (NdotM * NdotI));
                result.valid = uint(NdotO > 0.0f);
            }
            result.valid *= uint(NdotI > 0.0f) * uint(NdotM > 0.0f);
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
            float NdotM = dot(n, m);

            float F = fresnelDielectric(wI, m, etaI, etaO);
            if (randomSample.componentSample >= F)
            {
                float cosThetaI = -dot(m, wI);
                float sinThetaTSquared = sqr(etaI / etaO) * (1.0 - cosThetaI * cosThetaI);
                float cosThetaT = sqrt(saturate(1.0 - sinThetaTSquared));
                wO = wI * (etaI / etaO) + m * (etaI / etaO * cosThetaI - cosThetaT);
                float NdotO = dot(n, wO);

                float MdotI = abs(dot(m, wI));
                float MdotO = abs(dot(m, wO));
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                float J = MdotO / sqr(etaI * MdotI + etaO * MdotO);

                result.bsdf = material.transmittance *
                    (1.0f - F) * D * G * MdotI * MdotO / (NdotI * sqr(etaI * MdotI + etaO * MdotO));

                result.pdf = D * NdotM * J * (1.0f - F);
                result.weight = material.transmittance * (G * MdotI / (NdotI * NdotM));
                result.valid = uint(NdotI * NdotO < 0.0f);
            }
            else
            {
                wO = reflect(wI, m);
                float NdotO = dot(n, wO);

                float MdotO = dot(m, wO);
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                float J = 1.0f / (4.0 * MdotO);

                result.bsdf = material.specular * (F * D * G / (4.0 * NdotI));
                result.pdf = D * NdotM * J * F;
                result.weight = material.specular * (G * MdotO / (NdotM * NdotI));
                result.valid = uint(NdotO * NdotI > 0.0f);
            }
            break;
        }

        default:
        {
            break;
        }
    }

    result.direction = wO;
    result.valid *= uint(dot(result.bsdf, result.bsdf) > 0.0f) * uint(result.pdf > 0.0f) * uint(!isinf(result.pdf));

    return result;
}
