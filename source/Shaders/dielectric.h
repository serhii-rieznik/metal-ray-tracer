//
//  materials.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "structures.h"

namespace dielectric
{

inline SampledMaterial evaluate(device const Material& material, float3 nO, float3 wI, float3 wO)
{
    SampledMaterial result = { };
    result.direction = wO;

    bool enteringMaterial = dot(nO, wI) < 0.0f;
    float3 n = enteringMaterial ? nO : -nO;

    float NdotI = -dot(n, wI);
    float NdotO = dot(n, wO);
    bool reflection = NdotO * NdotI > 0.0f;

    float eta = enteringMaterial ?
        (material.extIOR.samples[0] / material.intIOR_eta.samples[0]) :
        (material.intIOR_eta.samples[0] / material.extIOR.samples[0]);

    float3 m = normalize(reflection ? (wO - wI) : (wI * eta - wO));
    m *= (dot(n, m) < 0.0f) ? -1.0f : 1.0f;

    float MdotI = -dot(m, wI);
    float MdotO = dot(m, wO);
    float NdotM = dot(n, m);
    float alpha = remapRoughness(material.roughness, NdotI);

    float F = fresnelDielectric(wI, m, eta);
    float D = ggxNormalDistribution(alpha, n, m);
    float G = ggxVisibilityTerm(alpha, wI, wO, n, m);

    if (reflection)
    {
        float J = 1.0f / (4.0f * MdotO);

        result.bsdf = material.specular * (D * G * F / (4.0f * NdotI));
        result.pdf = F * J;
        result.weight = material.specular;
    }
    else
    {
        float J = MdotI / sqr(MdotI * eta + MdotO);

        float value = (1.0f - F) * D * G * MdotI * MdotO / (NdotI * sqr(MdotI * eta + MdotO));
        result.bsdf = material.transmittance * abs(value);
        result.pdf = (1.0f - F) * J;

        result.weight = material.transmittance;
    }

    result.pdf *= D * NdotM;
    result.weight = result.weight * abs(G * MdotI / (NdotI * NdotM));
    result.valid = uint(GPUSpectrumMax(result.bsdf) > 0.0f) * uint(result.pdf > 0.0f);

    return result;
}

inline SampledMaterial sample(device const Material& material, float3 nO, float3 wI, device const RandomSample& randomSample)
{
    bool enteringMaterial = dot(nO, wI) < 0.0f;
    float3 n = enteringMaterial ? nO : -nO;
    float a = remapRoughness(material.roughness, dot(n, wI));
    float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, a);

    GPUSpectrum etaI = enteringMaterial ? material.extIOR : material.intIOR_eta;
    GPUSpectrum etaO = enteringMaterial ? material.intIOR_eta : material.extIOR;
    GPUSpectrum eta = etaI / etaO;

    float F = fresnelDielectric(wI, m, eta.samples[0]);

    float3 wO = { };
    if (randomSample.componentSample > F)
    {
        /*
         * Refraction
         *                   |
         *        m          |          m
         * wI \   +          |          +   + wO
         *     \  |          |          |  /
         *      \ |          |          | /
         *       +|          |          |/
         * ---------------   |   ---------------
         *         \         |         +
         *          \        |        /
         *           \       |       /
         *            + wO   |   wI /
         */
        float cosThetaI = dot(m, -wI);
        float sinThetaOSquared = (eta.samples[0] * eta.samples[0]) * (1.0f - cosThetaI * cosThetaI);
        float cosThetaO = sqrt(saturate(1.0f - sinThetaOSquared));
        wO = normalize(eta.samples[0] * wI + m * (eta.samples[0] * cosThetaI - cosThetaO));

        if (dot(n, wI) * dot(n, wO) <= 0.0f) return {};
    }
    else
    {
        /*
         * Reflection:
         *
         *        m
         * wI  \   +   +  wO
         *      \  |  /
         *       \ | /
         *        +|/
         * -----------------
         * (n, wI) < 0.0f
         * (n, wO) > 0.0f
         */
        wO = reflect(wI, m);

        if (dot(n, wO) * dot(n, wI) >= 0.0f) return {};
    }

    return evaluate(material, nO, wI, wO);
}

}
