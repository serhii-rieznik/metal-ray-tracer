//
//  materials.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "diffuse.h"
#include "conductor.h"
#include "plastic.h"
#include "dielectric.h"

inline float rand(float n) { return fract(sin(n) * 43758.5453123); }
inline float rand(float2 n) { return fract(sin(dot(n, float2(12.9898, 4.1414))) * 43758.5453); }

inline float noise(float p) {
    float fl = floor(p);
    float fc = fract(p);
    return mix(rand(fl), rand(fl + 1.0), fc);
}

inline float noise(float2 n) {
    const float2 d = float2(0.0, 1.0);
    float2 b = floor(n), f = smoothstep(float2(0.0), float2(1.0), fract(n));
    return mix(mix(rand(b), rand(b + d.yx), f.x), mix(rand(b + d.xy), rand(b + d.yy), f.x), f.y);
}

inline float mod289(float x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
inline float4 mod289(float4 x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
inline float4 perm(float4 x){return mod289(((x * 34.0) + 1.0) * x);}

inline float noise(float3 p){
    float3 a = floor(p);
    float3 d = p - a;
    d = d * d * (3.0 - 2.0 * d);

    float4 b = a.xxyy + float4(0.0, 1.0, 0.0, 1.0);
    float4 k1 = perm(b.xyxy);
    float4 k2 = perm(k1.xyxy + b.zzww);

    float4 c = k2 + a.zzzz;
    float4 k3 = perm(c);
    float4 k4 = perm(c + 1.0);

    float4 o1 = fract(k3 * (1.0 / 41.0));
    float4 o2 = fract(k4 * (1.0 / 41.0));

    float4 o3 = o2 * d.z + o1 * (1.0 - d.z);
    float2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

    return o4.y * d.y + o4.x * (1.0 - d.y);
}

inline SampledMaterial evaluateMaterial(device const Material& material, float3 p, float3 nO, float3 wI, float3 wO, float wavelength)
{
    SampledMaterial result;
    switch (material.type)
    {
        case MATERIAL_DIFFUSE:
            result = diffuse::evaluate(material, nO, wI, wO, wavelength);
            break;

        case MATERIAL_CONDUCTOR:
            result = conductor::evaluate(material, nO, wI, wO, wavelength);
            break;

        case MATERIAL_PLASTIC:
            result = plastic::evaluate(material, nO, wI, wO, wavelength);
            break;

        case MATERIAL_DIELECTRIC:
            result = dielectric::evaluate(material, nO, wI, wO, wavelength);
            break;

        default:
            return {};
    }

    if (material.coatingThickness > 0.0f)
    {
        float t = length(p.xz);// + noise(p * 13.0f + wavelength / 13.0f);
        float d = material.coatingThickness + t * 200.0f;
        float3 n = nO; // result.normal;
        float cosThetaI = abs(dot(wI, n));
        float cosThetaO = abs(dot(wO, n));
        float lengthWi = d / cosThetaI;
        float lengthWo = d / cosThetaO;
        float opticalPath =
            lengthWi + lengthWo + 0.5f * wavelength;
            // 2.0f * d * material.coatingIOR * cosThetaI; // lengthWi + lengthWo;
        float scale = 2.0f * fract(opticalPath / wavelength - 0.5f);
        result.weight *= scale;
        result.bsdf *= scale;
    }

    return result;
}

inline SampledMaterial sampleMaterial(device const Material& material, float3 p, float3 nO, float3 wI,
    device const RandomSample& randomSample, float wavelength)
{
    float3 wO;
    switch (material.type)
    {
        case MATERIAL_DIFFUSE:
            wO = diffuse::sample(material, nO, wI, randomSample, wavelength);
            break;

        case MATERIAL_CONDUCTOR:
            wO = conductor::sample(material, nO, wI, randomSample, wavelength);
            break;

        case MATERIAL_PLASTIC:
            wO = plastic::sample(material, nO, wI, randomSample, wavelength);
            break;

        case MATERIAL_DIELECTRIC:
            wO = dielectric::sample(material, nO, wI, randomSample, wavelength);
            break;

        default:
            return {};
    }

    return evaluateMaterial(material, p, nO, wI, wO, wavelength);
}
