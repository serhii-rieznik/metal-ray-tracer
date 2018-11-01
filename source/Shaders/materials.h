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
    float etaI = 1.0f;
    float etaO = 1.5f;

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
            if (NdotO > 0.0f)
            {
                wI = -wI;
                float3 m = normalize(wI + wO);
                float NdotM = dot(n, m);
                float NdotI = dot(n, wI);
                float MdotO = dot(m, wO);
                float3 F = material.specular * fresnelConductor(MdotO);
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                result.bsdfSpecular = F * (D * G / (4.0 * NdotI));
                result.pdfSpecular = D * NdotM / (4.0 * MdotO);
            }
            break;
        }

        case MATERIAL_PLASTIC:
        {
            float NdotO = dot(n, wO);
            if (NdotO > 0.0f)
            {
                wI = -wI;
                float3 m = normalize(wI + wO);
                float NdotM = dot(n, m);
                float NdotI = dot(n, wI);
                float NdotO = dot(n, wO);
                float MdotO = dot(m, wO);
                float3 F = material.specular * fresnelDielectric(MdotO, etaI, etaO);
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
            float NdotI = dot(n, wI);
            float NdotO = dot(n, wO);

            if ((NdotI * NdotO < 0.0f)) // ray is reflected
            {
                wI = -wI;
                NdotI = -NdotI;
                float3 m = normalize(wI + wO);
                float NdotM = dot(n, m);
                float MdotO = dot(m, wO);
                float3 F = material.specular * fresnelDielectric(MdotO, etaI, etaO);
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                float J = 1.0 / (4.0 * MdotO);
                result.pdfSpecular = D * NdotM * J;
                result.bsdfSpecular = material.specular * (F * D * G / (4.0 * NdotI));
            }
            else
            {
                if (NdotI < 0.0f) // exiting material
                {
                    NdotI = -NdotI;
                    float etaI_ = etaI;
                    etaI = etaO;
                    etaO = etaI_;
                }
                float3 m = normalize(wI * etaI + wO * etaO);
                float MdotO = abs(dot(m, wO));
                float MdotI = abs(dot(m, wI));
                float NdotM = abs(dot(n, m));
                float D = ggxNormalDistribution(alpha, NdotM);
                float G = ggxVisibilityTerm(alpha, wI, wO, m);
                float F = fresnelDielectric(wI, m, etaI, etaO);

                float J = sqr(etaI / etaO) * MdotO / sqr(etaI * MdotI + etaO * MdotO);
                result.pdfTransmittance = D * NdotM * J;

                float t1 = (MdotI * MdotO) / (NdotI * NdotO);
                float t2 = sqr(etaI / etaO) * (1.0f - F) * D * G / sqr(etaI * MdotI + etaO * MdotO);
                result.bsdfTransmittance = material.transmittance * t1 * t2;
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

SampledMaterial sampleMaterial(device const Material& material, float3 n, float3 wI, device const float4& randomSample)
{
    float etaI = 1.0f;
    float etaO = 1.5f;
    float pdf = 0.0f;
    float alpha = material.roughness * material.roughness;

    float3 wO = {};
    float3 weight = 0.0f;

    switch (material.type)
    {
        case MATERIAL_CONDUCTOR:
        {
            float3 m = sampleGGXDistribution(n, randomSample.wz, material.roughness);
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
            float3 m = sampleGGXDistribution(n, randomSample.wz, material.roughness);
            float F = fresnelDielectric(wI, m, etaI, etaO);

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
                    pdf = pdfSpecular * D * NdotM * J;
                    weight = material.specular * (F * G * MdotO) / (NdotI * NdotM * pdfSpecular);
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

            float3 m = sampleGGXDistribution(n, randomSample.wz, material.roughness);
            float pdfSpecular = fresnelDielectric(wI, m, etaI, etaO);
            float pdfTransmission = 1.0f - pdfSpecular;

            if (randomSample.y < pdfTransmission)
            {
                float cosThetaI = -dot(m, wI);
                float sinThetaTSquared = sqr(etaI / etaO) * (1.0 - cosThetaI * cosThetaI);
                float cosThetaT = sqrt(saturate(1.0 - sinThetaTSquared));
                wO = wI * (etaI / etaO) + m * (etaI / etaO * cosThetaI - cosThetaT);

                float3 h = normalize(wO * etaO - wI * etaI);

                float HdotI = abs(dot(h, wI));
                float HdotO = abs(dot(h, wO));
                float NdotI = -dot(n, wI);
                float NdotM = dot(n, m);
                float G = ggxVisibilityTerm(alpha, wI, wO, n);
                float D = ggxNormalDistribution(alpha, NdotM);
                float J = sqr(etaO) * HdotO / sqr(etaI * HdotI + etaO * HdotO);
                pdf = pdfTransmission * D * NdotM * J;
                weight = material.transmittance * HdotI * G / (NdotM * NdotI);
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
                    pdf = pdfSpecular * D * NdotM * J;
                    weight = material.specular * (G * MdotO) / (NdotI * NdotM);
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

/*
 * BTDF:
 *
 float t1 = (HdotI * HdotO) / (NdotI * NdotO);
 float t2 = sqr(etaO) * (1.0f - F) * D * G / sqr(etaI * HdotI + etaO * HdotO);
 float btdf = t1 * t2;
//*/

/*
void t()
{
    wI = -wI;
    float3 m = sampleGGXDistribution(n, randomSample.wz, material.roughness);
    float NdotI = dot(n, wI);
    float NdotM = dot(n, m);
    float D = ggxNormalDistribution(alpha, NdotM);

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

        n = -n;
    }
}
// */

/*/
{
    float3 h = normalize(wI * etaI + wO * etaO);
    float G = ggxVisibilityTerm(alpha, wI, wO, h);
    float NdotO = abs(dot(n, wO));
    float HdotO = abs(dot(h, wO));
    float HdotI = abs(dot(h, wI));

    float J = sqr(etaO) * HdotI / sqr(etaI * HdotI + etaO * HdotO);
    pdf = pdfTransmission * D * NdotM * J;

    float btdf = (HdotI * HdotO * sqr(etaO) * (1.0f - F) * D * G)
    / (sqr(etaI * HdotI + etaO * HdotO) * NdotI * NdotO);

    weight = btdf / pdf * NdotO;
}
// */
