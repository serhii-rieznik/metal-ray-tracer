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

inline SampledMaterial evaluate(device const Material& material, float3 nO, float3 wI, float3 wO)
{
    SampledMaterial result = { wO };
    float NdotO = dot(nO, wO);
    float NdotI = -dot(nO, wI);
    result.valid = uint(NdotO * NdotI > 0.0f) * (NdotO > 0.0f) * (NdotI > 0.0f);
    if (result.valid)
    {
        float3 m = normalize(wO - wI);
        float NdotM = dot(nO, m);
        float MdotO = dot(m, wO);

        float a = material.roughness * material.roughness;
        float F = fresnelConductor(wI, m, material.extIOR, material.intIOR);
        float D = ggxNormalDistribution(a, nO, m);
        float G = ggxVisibilityTerm(a, wI, wO, nO, m);
        float J = 1.0f / (4.0 * MdotO);
        result.bsdf = material.specular * (F * D * G / (4.0 * NdotI));
        result.pdf = D * NdotM * J;
        result.weight = material.specular * (F * G * MdotO / (NdotM * NdotI));
        result.eta = 1.0f;
    }
    return result;
}

inline SampledMaterial sample(device const Material& material, float3 nO, float3 wI, device const RandomSample& randomSample)
{
    float alphaSquared = material.roughness * material.roughness;
    float3 m = sampleGGXDistribution(nO, randomSample.bsdfSample, alphaSquared);
    float3 wO = reflect(wI, m);
    return evaluate(material, nO, wI, wO);
}

}
