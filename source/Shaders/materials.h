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
    float etaI = material.extIOR;
    float etaO = material.intIOR;

    SampledMaterialProperties result = {};

    float alpha = material.roughness * material.roughness;

    switch (material.type)
    {
        case MATERIAL_DIFFUSE:
        {
            float NdotO = dot(n, wO);
            if (NdotO > 0.0f)
            {
                result.bsdfDiffuse = material.diffuse * INVERSE_PI * NdotO;
                result.pdfDiffuse = INVERSE_PI * NdotO;
            }
            break;
        }

        case MATERIAL_CONDUCTOR:
        {
            float NdotO = dot(n, wO);
            float NdotI = -dot(n, wI);
            if ((NdotO > 0.0f) && (NdotI > 0.0f))
            {
                float3 m = normalize(wO - wI);
                float NdotM = dot(n, m);
                float MdotO = dot(m, wO);
                float F = fresnelConductor(MdotO);
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                result.bsdfSpecular = material.specular * F * (D * G / (4.0 * NdotI));
                result.pdfSpecular = D * NdotM / (4.0 * MdotO);
            }
            break;
        }

        case MATERIAL_PLASTIC:
        {
            float NdotO = dot(n, wO);
            float NdotI = -dot(n, wI);
            if ((NdotO > 0.0f) && (NdotI > 0.0f))
            {
                float3 m = normalize(wO - wI);
                float NdotM = dot(n, m);
                float NdotO = dot(n, wO);
                float MdotO = dot(m, wO);
                float F = fresnelDielectric(MdotO, etaI, etaO);
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                result.bsdfSpecular = material.specular * (F * D * G / (4.0 * NdotI));
                result.pdfSpecular = D * NdotM / (4.0 * MdotO);
                result.bsdfDiffuse = material.diffuse * (1.0 - F) * INVERSE_PI * NdotO;
                result.pdfDiffuse = INVERSE_PI * NdotO;
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
            alpha = (1.2f - 0.2f * sqrt(abs(NdotI))) * material.roughness;
            alpha *= alpha;

            float NdotO = dot(n, wO);
            float F = fresnelDielectric(wI, n, etaI, etaO);

            if (NdotI * NdotO > 0.0f) // ray is reflected
            {
                float3 m = normalize(wO - wI);
                float NdotM = abs(dot(n, m));
                float MdotO = abs(dot(m, wO));
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                float J = 1.0f / (4.0 * MdotO);
                result.pdfSpecular = D * NdotM * J;
                result.bsdfSpecular = material.specular * (F * D * G / (4.0 * NdotI));
            }
            else
            {
                result.pdfTransmittance = 0.0f;
                result.bsdfTransmittance = 0.0f;
            }

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

SampledMaterial sampleMaterial(device const Material& material, float3 n, float3 wI, device const RandomSample& randomSample)
{
    float etaI = material.extIOR;
    float etaO = material.intIOR;
    float pdf = 0.0f;
    float alpha = material.roughness * material.roughness;

    float3 wO = {};
    float3 weight = 0.0f;

    switch (material.type)
    {
        case MATERIAL_CONDUCTOR:
        {
            float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, material.roughness);
            wO = reflect(wI, m);
            float NdotI = -dot(n, wI);
            float NdotO = dot(n, wO);
            if ((NdotO > 0.0f) && (NdotI > 0.0f))
            {
                float NdotM = dot(n, m);
                float MdotO = dot(m, wO);
                float F = fresnelConductor(MdotO);
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                pdf = D * NdotM / (4.0 * MdotO);
                weight = material.specular * F * G * MdotO / (NdotI * NdotM);
            }
            break;
        }

        case MATERIAL_PLASTIC:
        {
            float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, material.roughness);

            float pdfSpecular = fresnelDielectric(wI, m, etaI, etaO);
            float pdfDiffuse = 1.0f - pdfSpecular;

            if (randomSample.componentSample < pdfDiffuse)
            {
                wO = sampleCosineWeightedHemisphere(n, randomSample.bsdfSample);
                pdf = /* pdfDiffuse */ INVERSE_PI * dot(n, wO);
                weight = material.diffuse;
            }
            else
            {
                wO = reflect(wI, m);
                float NdotI = -dot(n, wI);
                float NdotO = dot(n, wO);
                if ((NdotO > 0.0f) && (NdotI > 0.0f))
                {
                    float NdotM = dot(n, m);
                    float MdotO = dot(m, wO);
                    float D = ggxNormalDistribution(alpha, NdotM);
                    float G = ggxVisibilityTerm(alpha, wI, wO, n);
                    float J = 1.0f / (4.0 * MdotO);
                    pdf = /* pdfSpecular */ D * NdotM * J;
                    weight = material.specular * (G * MdotO) / (NdotI * NdotM);
                }
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
            alpha = (1.2f - 0.2f * sqrt(abs(NdotI))) * material.roughness;
            alpha *= alpha;

            float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, material.roughness);
            float pdfSpecular = fresnelDielectric(wI, m, etaI, etaO);
            float pdfTransmission = 1.0f - pdfSpecular;

            if (randomSample.componentSample < pdfTransmission)
            {
                float cosThetaI = -dot(m, wI);
                float sinThetaTSquared = sqr(etaI / etaO) * (1.0 - cosThetaI * cosThetaI);
                float cosThetaT = sqrt(saturate(1.0 - sinThetaTSquared));
                wO = wI * (etaI / etaO) + m * (etaI / etaO * cosThetaI - cosThetaT);

                float3 h = (abs(etaO - etaI) < DISTANCE_EPSILON) ? m : normalize(wO * etaO - wI * etaI);

                // float HdotO = abs(dot(h, wO));
                float HdotI = abs(dot(h, wI));
                float NdotM = dot(n, m);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                // float D = ggxNormalDistribution(alpha, NdotM);
                // float J = HdotO / sqr(etaI * HdotI + etaO * HdotO);
                pdf = -1.0f; // disable MIS for now, later use: /* pdfTransmission */ D * NdotM * J;
                weight = material.transmittance * HdotI * G / (NdotM * NdotI);
            }
            else
            {
                wO = reflect(wI, m);
                float NdotO = dot(n, wO);
                if ((NdotO > 0.0f) && (NdotI > 0.0f))
                {
                    float NdotM = dot(n, m);
                    float MdotO = dot(m, wO);
                    float D = ggxNormalDistribution(alpha, NdotM);
                    float G = ggxVisibilityTerm(alpha, wI, wO, n);
                    float J = 1.0f / (4.0 * MdotO);
                    pdf = /* pdfSpecular */ D * NdotM * J;
                    weight = material.specular * (G * MdotO) / (NdotI * NdotM);
                }
            }
            break;
        }

        default:
        {
            wO = sampleCosineWeightedHemisphere(n, randomSample.bsdfSample);
            pdf = INVERSE_PI * dot(n, wO);
            weight = material.diffuse;
            break;
        }
    }

    return { wO, pdf, weight };
}

/*
 * BTDF:
 *
 float t1 = (HdotI * HdotO) / (NdotI * NdotO);
 float t2 = sqr(etaO) * (1.0f - F) * D * G / sqr(etaI * HdotI + etaO * HdotO);
 float btdf = t1 * t2;
//*/
