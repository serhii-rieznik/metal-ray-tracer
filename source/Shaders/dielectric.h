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

inline SampledMaterial evaluate(device const Material& material, float3 nO, float3 wI, float3 wO, float wavelength)
{
    SampledMaterial result = { };
    result.direction = wO;

    bool enteringMaterial = dot(nO, wI) < 0.0f;
    float3 n = enteringMaterial ? nO : -nO;

    float NdotI = -dot(n, wI);
    float NdotO = dot(n, wO);
    bool reflection = NdotO * NdotI > 0.0f;

    float extIOR = GPUSpectrumSample(material.extIOR, wavelength);
    float intIOR = GPUSpectrumSample(material.intIOR_eta, wavelength);
    float eta = enteringMaterial ? (extIOR / intIOR) : (intIOR / extIOR);

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

        float specularSample = GPUSpectrumSample(material.specular, wavelength);
        result.bsdf = specularSample * (D * G * F / (4.0f * NdotI));
        result.pdf = F * J;
        result.weight = specularSample;
    }
    else
    {
        float J = MdotI / sqr(MdotI * eta + MdotO);
        float value = (1.0f - F) * D * G * MdotI * MdotO / (NdotI * sqr(MdotI * eta + MdotO));
        float transmittanceSample = GPUSpectrumSample(material.transmittance, wavelength);
        result.bsdf = transmittanceSample * abs(value);
        result.pdf = (1.0f - F) * J;
        result.weight = transmittanceSample;
    }

    result.pdf *= D * NdotM;
    result.weight = result.weight * abs(G * MdotI / (NdotI * NdotM));
    result.valid = uint(result.bsdf > 0.0f) * uint(result.pdf > 0.0f);

    return result;
}

inline SampledMaterial sample(device const Material& material, float3 nO, float3 wI,
    device const RandomSample& randomSample, float wavelength)
{
    bool enteringMaterial = dot(nO, wI) < 0.0f;
    float3 n = enteringMaterial ? nO : -nO;
    float a = remapRoughness(material.roughness, dot(n, wI));
    float3 m = sampleGGXDistribution(n, randomSample.bsdfSample, a);

    float extIOR = GPUSpectrumSample(material.extIOR, wavelength);
    float intIOR = GPUSpectrumSample(material.intIOR_eta, wavelength);
    float eta = enteringMaterial ? (extIOR / intIOR) : (intIOR / extIOR);

    float F = fresnelDielectric(wI, m, eta);

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
        float sinThetaOSquared = (eta * eta) * (1.0f - cosThetaI * cosThetaI);
        float cosThetaO = sqrt(saturate(1.0f - sinThetaOSquared));
        wO = normalize(eta * wI + m * (eta * cosThetaI - cosThetaO));

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

    return evaluate(material, nO, wI, wO, wavelength);
}

}
