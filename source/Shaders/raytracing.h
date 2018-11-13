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

inline float lightSamplingPdf(float3 n, float3 l, float area)
{
    float distanceSquared = dot(l, l);
    float cosTheta = -dot(n, l) / sqrt(distanceSquared);
    return (cosTheta > 0.0f) ? (distanceSquared / (cosTheta * area)) : 0.0f;
}

inline LightSample sampleLight(float3 origin, float3 normal, device const EmitterTriangle* emitterTriangles,
    uint emitterTrianglesCount, device const RandomSample& randomSample)
{
    LightSample lightSample = {};
    device const EmitterTriangle& emitterTriangle =
        sampleEmitterTriangle(emitterTriangles, emitterTrianglesCount, randomSample.emitterSample);

    float3 lightTriangleBarycentric = barycentric(randomSample.barycentricSample);
    Vertex lightVertex = interpolate(emitterTriangle.v0, emitterTriangle.v1, emitterTriangle.v2, lightTriangleBarycentric);
    float3 wO = lightVertex.v - origin;

    lightSample.position = lightVertex.v;
    lightSample.direction = normalize(wO);
    lightSample.pdf = emitterTriangle.pdf * lightSamplingPdf(lightVertex.n, wO, emitterTriangle.area);
    lightSample.value = emitterTriangle.emissive / lightSample.pdf;
    lightSample.primitiveIndex = emitterTriangle.globalIndex;

    lightSample.pdf = isinf(lightSample.pdf) ? 0.0f : lightSample.pdf;
    lightSample.valid = (lightSample.pdf > 0.0f);

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
    return saturate(f2 / (f2 + g2));
}

inline float ggxNormalDistribution(float alphaSquared, float cosTheta)
{
    float cosThetaSquared = sqr(cosTheta);
    float denom = cosThetaSquared * (alphaSquared - 1.0f) + 1.0f;
    return float(cosTheta > 0.0f) * INVERSE_PI * alphaSquared / (denom * denom);
}

inline float ggxVisibility(float alphaSquared, float cosTheta)
{
    float cosThetaSquared = sqr(cosTheta);
    float tanThetaSquared = (1.0f - cosThetaSquared) / cosThetaSquared;
    return 2.0f / (1.0f + sqrt(1.0f + alphaSquared * tanThetaSquared));
}

inline float ggxVisibilityTerm(float alphaSquared, float3 wI, float3 wO, float3 m)
{
    float MdotI = dot(m, wI);
    float MdotO = dot(m, wO);
    return ggxVisibility(alphaSquared, MdotI) * ggxVisibility(alphaSquared, MdotO);
}

inline float FresnelConductor(float3 i, float3 m, float etaI, float etaO)
{
    return 1.0f;
}

inline float fresnelDielectric(float3 i, float3 m, float etaI, float etaO)
{
    float result = 1.0f;
    float cosThetaI = abs(dot(i, m));
    float sinThetaOSquared = sqr(etaI / etaO) * (1.0f - cosThetaI * cosThetaI);
    if (sinThetaOSquared <= 1.0)
    {
        float cosThetaO = sqrt(saturate(1.0f - sinThetaOSquared));
        float Rs = (etaI * cosThetaI - etaO * cosThetaO) / (etaI * cosThetaI + etaO * cosThetaO);
        float Rp = (etaO * cosThetaI - etaI * cosThetaI) / (etaO * cosThetaI + etaI * cosThetaI);
        result = 0.5f * (Rs * Rs + Rp * Rp);
    }
    return result;
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

inline bool evaluateDiffuseReflection(device const Material& material, float3 n, float3 m, float3 wI, float3 wO,
    thread packed_float3& bsdf, thread float& pdf, thread packed_float3& weight)
{
    float NdotO = dot(n, wO);
    float scale = float(NdotO > 0.0f);
    {
        bsdf = scale * material.diffuse * INVERSE_PI * NdotO;
        pdf = scale * INVERSE_PI * NdotO;
        weight = scale * material.diffuse;
    }
    return (scale > 0.0f);
}

inline bool evaluateRoughDiffuseReflection(device const Material& material, float3 n, float3 m, float3 wI, float3 wO,
    thread packed_float3& bsdf, thread float& pdf, thread packed_float3& weight)
{
    float NdotO = dot(n, wO);
    float MdotO = dot(m, wO);
    float MdotI = -dot(m, wI);
    float fV = pow(1.0f - MdotI, 5.0f);
    float fL = pow(1.0f - MdotO, 5.0f);
    float scale = float(NdotO > 0.0f) * float(MdotO > 0.0f) * float(MdotI > 0.0f);
    {
        bsdf = scale * material.diffuse * INVERSE_PI * NdotO * (1.0f - 0.5f * fL) * (1.0f - 0.5f * fV);
        pdf = scale * INVERSE_PI * NdotO;
        weight = scale * material.diffuse * (1.0f - 0.5f * fL) * (1.0f - 0.5f * fV);
    }
    return (scale > 0.0f);
}

inline bool evaluateMicrofacetReflection(device const Material& material, float3 n, float3 m, float3 wI, float3 wO,
    thread const FresnelSample& F, thread packed_float3& bsdf, thread float& pdf, thread packed_float3& weight)
{
    float NdotO = dot(n, wO);
    float NdotM = dot(n, m);
    float MdotO = dot(m, wO);
    float NdotI = -dot(n, wI);
    float alpha = remapRoughness(material.roughness, NdotI);
    float D = ggxNormalDistribution(alpha, NdotM);
    float G = ggxVisibilityTerm(alpha, wI, wO, n);
    float J = 1.0f / (4.0 * MdotO);
    float scale = float(NdotO > 0.0f) * float(NdotI > 0.0f);
    {
        bsdf = material.specular * (scale * F.value * D * G / (4.0 * NdotI));
        pdf = scale * D * NdotM * J;
        weight = material.specular * (scale * F.value * G * MdotO / (NdotM * NdotI));
    }
    return (NdotO > 0.0f) && (NdotI > 0.0f);
}

inline bool evaluateMicrofacetTransmission(device const Material& material, float3 n, float3 m, float3 wI, float3 wO,
    thread const FresnelSample& F, thread packed_float3& bsdf, thread float& pdf, thread packed_float3& weight)
{
    float NdotI = -dot(n, wI);
    float NdotO = dot(n, wO);
    float NdotM = dot(n, m);

    if ((F.value >= 1.0f) || (NdotI * NdotO >= 0.0f))
        return false;

    float alpha = remapRoughness(material.roughness, NdotI);

    float3 h = normalize(wO * F.etaO - wI * F.etaI);
    float HdotI = abs(dot(h, wI));
    float HdotO = abs(dot(h, wO));
    float D = ggxNormalDistribution(alpha, NdotM);
    float G = ggxVisibilityTerm(alpha, wI, wO, n);
    float J = HdotO / sqr(F.etaI * HdotI + F.etaO * HdotO);

    bsdf = material.transmittance *
        (1.0f - F.value) * D * G * HdotI * HdotO / (NdotI * sqr(F.etaI * HdotI + F.etaO * HdotO));

    pdf = D * NdotM * J;

    weight = material.transmittance * (1.0f - F.value) * G * HdotI / (NdotI * NdotM);

    return (pdf > 0.0f);
}
