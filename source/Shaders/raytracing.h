//
//  raytracing.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "structures.h"

inline Vertex interpolate(device const Vertex& a, device const Vertex& b, device const Vertex& c, float3 barycentric)
{
    float u = barycentric.x;
    float v = barycentric.y;
    float w = barycentric.z;

    Vertex result;
    result.v = a.v * u + b.v * v + c.v * w;
    result.n = normalize(a.n * u + b.n * v + c.n * w);
    result.t = a.t * u + b.t * v + c.t * w;
    return result;
}

inline Vertex interpolate(device const Vertex& a, device const Vertex& b, device const Vertex& c, float2 barycentric)
{
    float u = barycentric.x;
    float v = barycentric.y;
    float w = saturate(1.0f - barycentric.x - barycentric.y);

    Vertex result;
    result.v = a.v * u + b.v * v + c.v * w;
    result.n = normalize(a.n * u + b.n * v + c.n * w);
    result.t = a.t * u + b.t * v + c.t * w;
    return result;
}

inline device const EmitterTriangle& sampleEmitterTriangle(device const EmitterTriangle* triangles, uint triangleCount, float xi)
{
    uint l = 0;
    uint r = triangleCount - 1;

    bool continueSearch = true;
    do
    {
        uint m = (l + r) / 2;
        float c = triangles[m + 1].cdf;
        uint setRight = uint32_t(c > xi);
        uint setLeft = uint32_t(c < xi);
        r -= setRight * (r - m);
        l += setLeft * ((m + 1) - l);
        continueSearch = (l < r) && (setRight + setLeft > 0);
    } while (continueSearch);

    return triangles[l];
}

inline float3 barycentric(float2 smp)
{
    float r1 = sqrt(smp.x);
    float r2 = smp.y;
    return float3(1.0f - r1, r1 * (1.0f - r2), r1 * r2);
}

inline float directSamplingPdf(float3 n, float3 l, float area)
{
    float distanceSquared = dot(l, l);
    float cosTheta = -dot(n, l) / sqrt(distanceSquared);
    return (cosTheta > 0.0f) ? (distanceSquared / (cosTheta * area)) : 0.0f;
}

inline LightSample sampleLight(float3 origin, float3 normal, device const EmitterTriangle* emitterTriangles,
    uint emitterTrianglesCount, device const RandomSample& randomSample)
{
    LightSample lightSample = {};
    
    device const EmitterTriangle& emitter  =
        sampleEmitterTriangle(emitterTriangles, emitterTrianglesCount, randomSample.emitterSample);

    float3 lightTriangleBarycentric = barycentric(randomSample.barycentricSample);
    Vertex lightVertex = interpolate(emitter.v0, emitter.v1, emitter.v2, lightTriangleBarycentric);
    float3 wO = lightVertex.v - origin;

    lightSample.direction = normalize(wO);
    lightSample.samplePdf = emitter.discretePdf * directSamplingPdf(lightVertex.n, wO, emitter.area);
    lightSample.primitiveIndex = emitter.globalIndex;

    lightSample.valid = (dot(normal, wO) > 0.0f) && (dot(float3(lightVertex.n), wO) < 0.0f) && (lightSample.samplePdf > 0.0f);
        // (lightSample.samplePdf > 0.0f) && (dot(wO, float3(lightVertex.n)) < 0.0f) && (dot(wO, normal) > 0.0f);

    lightSample.value = lightSample.valid ? (emitter.emissive / lightSample.samplePdf) : 0.0f;

    return lightSample;
}

inline void buildOrthonormalBasis(float3 n, thread float3& u, thread float3& v)
{
    float s = (n.z < 0.0 ? -1.0f : 1.0f);
    float a = -1.0f / (s + n.z);
    float b = n.x * n.y * a;
    u = float3(1.0f + s * n.x * n.x * a, s * b, -s * n.x);
    v = float3(b, s + n.y * n.y * a, -n.y);
}

inline float sqr(float a)
{
    return a * a;
}

inline float3 alignToDirection(float3 n, float cosTheta, float phi)
{
    float sinTheta = sqrt(saturate(1.0f - cosTheta * cosTheta));

    float3 u;
    float3 v;
    buildOrthonormalBasis(n, u, v);

    return (u * cos(phi) + v * sin(phi)) * sinTheta + n * cosTheta;
}

inline float3 sampleCosineWeightedHemisphere(float3 n, float2 xi)
{
    float cosTheta = sqrt(xi.x);
    return alignToDirection(n, cosTheta, xi.y * DOUBLE_PI);
}

inline float3 sampleGGXDistribution(float3 n, float2 xi, float alphaSquared)
{
    float cosTheta = sqrt(saturate((1.0f - xi.x) / (xi.x * (alphaSquared - 1.0f) + 1.0f)));
    return alignToDirection(n, cosTheta, xi.y * DOUBLE_PI);
}

inline float powerHeuristic(float fPdf, float gPdf)
{
    float f2 = sqr(fPdf);
    float g2 = sqr(gPdf);
    return f2 / (f2 + g2);
}

inline float ggxNormalDistribution(float alphaSquared, float3 n, float3 m)
{
    float cosTheta = dot(n, m);
    float cosThetaSquared = sqr(cosTheta);
    float denom = cosThetaSquared * (alphaSquared - 1.0f) + 1.0f;
    return float(cosTheta > 0.0f) ? (alphaSquared / (PI * denom * denom)) : 0.0f;
}

inline float ggxVisibility(float alphaSquared, float3 w, float3 n, float3 m)
{
    float NdotW = dot(n, w);
    float MdotW = dot(m, w);
    if (MdotW * NdotW <= 0.0f)
        return 0.0f;

    float cosThetaSquared = NdotW * NdotW;
    float tanThetaSquared = (1.0f - cosThetaSquared) / cosThetaSquared;
    if (tanThetaSquared == 0.0f)
        return 1.0f;

    return 2.0f / (1.0f + sqrt(1.0f + alphaSquared * tanThetaSquared));
}

inline float ggxVisibilityTerm(float alphaSquared, float3 wI, float3 wO, float3 n, float3 m)
{
    return ggxVisibility(alphaSquared, wI, n, m) * ggxVisibility(alphaSquared, wO, n, m);
}

inline float fresnelConductor(float3 i, float3 m, float etaI, float etaO)
{
    return 1.0f;
}

inline float fresnelDielectric(float3 i, float3 m, float eta)
{
    float result = 1.0f;
    float cosThetaI = abs(dot(i, m));
    float sinThetaOSquared = (eta * eta) * (1.0f - cosThetaI * cosThetaI);
    if (sinThetaOSquared <= 1.0)
    {
        float cosThetaO = sqrt(saturate(1.0f - sinThetaOSquared));
        float Rs = (cosThetaI - eta * cosThetaO) / (cosThetaI + eta * cosThetaO);
        float Rp = (eta * cosThetaI - cosThetaO) / (eta * cosThetaI + cosThetaO);
        result = 0.5f * (Rs * Rs + Rp * Rp);
    }
    return result;
}

inline float fresnelDielectric(float3 i, float3 m, float etaI, float etaO)
{
    return fresnelDielectric(i, m, etaI / etaO);
}

inline float2 directionToEquirectangularCoordinates(float3 d)
{
    d = normalize(d);
    float u = atan2(d.x, d.z) / DOUBLE_PI;
    float v = 0.5 - asin(d.y) / PI;
    return float2(-u, v);
}

inline float3 sampleEnvironment(texture2d<float> environment, float3 d)
{
    constexpr sampler environmentSampler = sampler(s_address::repeat, t_address::clamp_to_edge, filter::linear);
    float2 uv = directionToEquirectangularCoordinates(d);
    return environment.sample(environmentSampler, uv).xyz;
}

inline float remapRoughness(float r, float NdotI)
{
    float alpha = (1.2f - 0.2f * sqrt(abs(NdotI))) * r;
    return alpha * alpha;
}
