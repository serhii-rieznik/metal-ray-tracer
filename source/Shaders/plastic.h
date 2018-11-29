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

inline SampledMaterial evaluate(device const Material& material, float3 nO, float3 wI, float3 wO)
{
    SampledMaterial result = { wO };
    float NdotI = -dot(nO, wI);
    float NdotO = dot(nO, wO);
    result.valid = uint(NdotO * NdotI > 0.0f) * uint(NdotO > 0.0f) * uint(NdotI > 0.0f);
    if (result.valid)
    {
        float3 m = normalize(wO - wI);
        float NdotM = dot(nO, m);
        float MdotO = dot(m, wO);

        float a = material.roughness * material.roughness;
        float F = fresnelDielectric(wI, m, material.extIOR, material.intIOR);
        float D = ggxNormalDistribution(a, nO, m);
        float G = ggxVisibilityTerm(a, wI, wO, nO, m);
        float J = 1.0f / (4.0 * MdotO);
        
        result.bsdf =
            material.diffuse * INVERSE_PI * NdotO +
            material.specular * (F * D * G / (4.0 * NdotI));

        result.pdf =
            INVERSE_PI * NdotO * (1.0f - F) +
            D * NdotM * J * F;

        result.weight = result.bsdf / result.pdf;
        result.eta = 1.0f;
    }
    return result;
}

inline SampledMaterial sample(device const Material& material, float3 nO, float3 wI, device const RandomSample& randomSample)
{
    float alphaSquared = material.roughness * material.roughness;
    float3 m = sampleGGXDistribution(nO, randomSample.bsdfSample, alphaSquared);
    float F = fresnelDielectric(wI, m, material.extIOR, material.intIOR);

    float3 wO = {};
    if (randomSample.componentSample < F)
    {
        wO = reflect(wI, m);
    }
    else
    {
        wO = sampleCosineWeightedHemisphere(nO, randomSample.bsdfSample);
    }

    return evaluate(material, nO, wI, wO);
}

}
