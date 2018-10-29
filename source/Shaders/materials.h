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
            float G = ggxVisibilityTerm(material.roughness * material.roughness, wI, wO, n);
            result.bsdf0 = F * (D * G / (4.0 * NdotI));
            result.pdf0 = D * NdotM / (4.0 * MdotO);
            break;
        }

        case MATERIAL_PLASTIC:
        {
            float3 F = material.specular * fresnelDielectric(MdotO, 1.0f, 1.5f);
            float D = ggxNormalDistribution(material.roughness * material.roughness, NdotM);
            float G = ggxVisibilityTerm(material.roughness * material.roughness, wI, wO, n);
            result.bsdf0 = F * (D * G / (4.0 * NdotI));
            result.pdf0 = D * NdotM / (4.0 * MdotO);

            result.bsdf1 = material.diffuse * (1.0 - F) * INVERSE_PI * NdotO;
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
    float3 wO = {};

    float3 m = sampleGGXDistribution(n, randomSample.wz, material.roughness);
    float3 r = reflect(wI, m);
    wI = -wI;

    float NdotM = dot(n, m);
    float NdotI = dot(n, wI);

    float a = material.roughness * material.roughness;
    float D = ggxNormalDistribution(a, NdotM);

    float pdf = 0.0f;
    float3 weight = 0.0f;

    switch (material.type)
    {
        case MATERIAL_CONDUCTOR:
        {
            wO = r;
            float MdotO = dot(m, wO);
            float F = fresnelConductor(MdotO);
            float G = ggxVisibilityTerm(a, wI, wO, n);
            pdf = D * NdotM / (4.0 * MdotO);
            weight = material.specular * F * G * MdotO / (NdotI * NdotM);
            break;
        }

        case MATERIAL_PLASTIC:
        {
            float F = fresnelDielectric(wI, m, 1.0f, 1.5f);

            float pdfSpecular = F;
            float pdfDiffuse = 1.0f - pdfSpecular;

            if (randomSample.y < pdfDiffuse)
            {
                wO = sampleCosineWeightedHemisphere(n, randomSample.wx);
                pdf = INVERSE_PI * dot(n, wO) * pdfDiffuse;
                weight = material.diffuse * (1.0 - F) / pdfDiffuse;
            }
            else
            {
                wO = r;
                float MdotO = dot(m, wO);
                float G = ggxVisibilityTerm(a, wI, wO, n);
                float J = 1.0f / (4.0 * MdotO);
                pdf = pdfSpecular * D * NdotM * J;
                weight = material.specular * (F * G * MdotO) / (NdotI * NdotM * pdfSpecular);
            }
            break;
        }

        case MATERIAL_DIELECTRIC:
        {
            float etaI = 1.0f;
            float etaO = 1.5f;

            if (NdotI <= 0.0f) // exiting material
            {
                n = -n;
                m = -m;
                NdotI = -NdotI;
                float etaK = etaI;
                etaI = etaO;
                etaO = etaK;
            }

            float etaR = etaI / etaO;
            float MdotI = dot(m, wI);
            float cosThetaI = MdotI;
            float sinThetaTSquared = sqr(etaR) * (1.0 - cosThetaI * cosThetaI);
            float F = fresnelDielectric(wI, m, etaI, etaO);

            float pdfSpecular = (sinThetaTSquared < 1.0f) ? F : 1.0f;
            float pdfTransmission = 1.0f - pdfSpecular;

            if (randomSample.y < pdfTransmission)
            {
                float cosThetaT = sqrt(saturate(1.0 - sinThetaTSquared));
                wO = m * (etaR * cosThetaI - cosThetaT) - etaR * wI;

                float3 h = normalize(wI * etaI + wO * etaO);
                float G = ggxVisibilityTerm(a, wI, wO, h);
                float HdotO = abs(dot(h, wO));
                float HdotI = abs(dot(h, wI));

                float J = sqr(etaO) * HdotI / sqr(etaI * HdotI + etaO * HdotO);
                pdf = pdfTransmission * D * NdotM * J;

                /*
                float t1 = (HdotI * HdotO) / (NdotI * NdotO);
                float t2 = sqr(etaO) * (1.0f - F) * D * G / sqr(etaI * HdotI + etaO * HdotO);
                float btdf = t1 * t2;
                // */

                weight = material.transmittance * (HdotO * G * (1.0 - F)) / (NdotI * NdotM * pdfTransmission);

                n = -n;
            }
            else
            {
                wO = r;
                float MdotO = dot(m, wO);
                float G = ggxVisibilityTerm(a, wI, wO, n);
                float J = 1.0 / (4.0 * MdotO);
                pdf = pdfSpecular * D * NdotM * J;
                weight = material.specular * (F * G * MdotO) / (NdotI * NdotM * pdfSpecular);
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

    if (!(dot(n, wO) > 0.0f))
    {
        pdf = 0.0f;
        weight = 0.0f;
    }

    return { wO, pdf, weight };
}
