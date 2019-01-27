//
//  materials.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "structures.h"

namespace plastic
{

inline SampledMaterial evaluate(device const Material& material, float3 nO, float3 wI, float3 wO, float wavelength)
{
    float NdotI = -dot(nO, wI);
    float NdotO = dot(nO, wO);
    
    SampledMaterial result = { };
    result.direction = wO;
    result.valid = uint(NdotO * NdotI > 0.0f) * uint(NdotO > 0.0f) * uint(NdotI > 0.0f);
    if (result.valid)
    {
        float3 m = normalize(wO - wI);
        float NdotM = dot(nO, m);
        float MdotO = dot(m, wO);
        float extIOR = GPUSpectrumSample(material.extIOR, wavelength);
        float intIOR = GPUSpectrumSample(material.intIOR_eta, wavelength);
        float a = material.roughness * material.roughness;
        float F = fresnelDielectric(wI, m, extIOR, intIOR);
        float D = ggxNormalDistribution(a, nO, m);
        float G = ggxVisibilityTerm(a, wI, wO, nO, m);
        float J = 1.0f / (4.0 * MdotO);
        
        result.bsdf =
            GPUSpectrumSample(material.diffuse, wavelength) * (INVERSE_PI * NdotO * (1.0f - F)) +
            GPUSpectrumSample(material.specular, wavelength) * (F * D * G / (4.0 * NdotI));

        result.pdf =
            INVERSE_PI * NdotO * (1.0f - F) +
            D * NdotM * J * F;

        result.weight = result.bsdf * (1.0f / result.pdf);
        
        result.normal = m;
    }
    return result;
}

inline float3 sample(device const Material& material, float3 nO, float3 wI,
    device const RandomSample& randomSample, float wavelength)
{
    float alphaSquared = material.roughness * material.roughness;
    float extIOR = GPUSpectrumSample(material.extIOR, wavelength);
    float intIOR = GPUSpectrumSample(material.intIOR_eta, wavelength);
    float3 m = sampleGGXDistribution(nO, randomSample.bsdfSample, alphaSquared);
    float F = fresnelDielectric(wI, m, extIOR, intIOR);

    float3 wO = {};
    if (randomSample.componentSample > F)
    {
        wO = sampleCosineWeightedHemisphere(nO, randomSample.bsdfSample);
    }
    else
    {
        wO = reflect(wI, m);
    }
    return wO;
}

}
