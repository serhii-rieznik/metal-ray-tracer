//
//  materials.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "structures.h"

namespace conductor
{

inline SampledMaterial evaluate(device const Material& material, float3 nO, float3 wI, float3 wO, float wavelength)
{
    float NdotO = dot(nO, wO);
    float NdotI = -dot(nO, wI);

    SampledMaterial result = { };
    result.direction = wO;
    result.valid = uint(NdotO * NdotI > 0.0f) * (NdotO > 0.0f) * (NdotI > 0.0f);
    if (result.valid)
    {
        float3 m = normalize(wO - wI);
        float NdotM = dot(nO, m);
        float MdotO = dot(m, wO);

        float a = material.roughness * material.roughness;
        float D = ggxNormalDistribution(a, nO, m);
        float G = ggxVisibilityTerm(a, wI, wO, nO, m);
        float J = 1.0f / (4.0 * MdotO);

        float specularSample = GPUSpectrumSample(material.specular, wavelength);
        float extIORSample = GPUSpectrumSample(material.extIOR, wavelength);
        float intIOREtaSample = GPUSpectrumSample(material.intIOR_eta, wavelength);
        float intIORKSample = GPUSpectrumSample(material.intIOR_k, wavelength);
        float F = fresnelConductor(wI, m, extIORSample, intIOREtaSample, intIORKSample);
        
        result.normal = m;
        result.bsdf = specularSample * F * (D * G / (4.0 * NdotI));
        result.pdf = D * NdotM * J;
        result.weight = specularSample * F * (G * MdotO / (NdotM * NdotI));
    }
    return result;
}

inline float3 sample(device const Material& material, float3 nO, float3 wI,
    device const RandomSample& randomSample, float wavelength)
{
    float alphaSquared = material.roughness * material.roughness;
    float3 m = sampleGGXDistribution(nO, randomSample.bsdfSample, alphaSquared);
    return reflect(wI, m);
}

}
