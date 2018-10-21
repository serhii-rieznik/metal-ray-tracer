//
//  raytracing.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include "structures.h"

Vertex interpolate(device const Vertex& a, device const Vertex& b, device const Vertex& c, float3 barycentric)
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

Vertex interpolate(device const Vertex& a, device const Vertex& b, device const Vertex& c, float2 barycentric)
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

device const EmitterTriangle& sampleEmitterTriangle(device const EmitterTriangle* triangles, uint triangleCount, float xi)
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

float3 barycentric(float2 smp)
{
    float r1 = sqrt(smp.x);
    float r2 = smp.y;
    return float3(1.0f - r1, r1 * (1.0f - r2), r1 * r2);
}

void buildOrthonormalBasis(float3 n, thread float3& u, thread float3& v)
{
    float s = (n.z < 0.0 ? -1.0f : 1.0f);
    float a = -1.0f / (s + n.z);
    float b = n.x * n.y * a;
    u = float3(1.0f + s * n.x * n.x * a, s * b, -s * n.x);
    v = float3(b, s + n.y * n.y * a, -n.y);
}

float3 alignToDirection(float3 n, float cosTheta, float phi)
{
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    float3 u;
    float3 v;
    buildOrthonormalBasis(n, u, v);

    return (u * cos(phi) + v * sin(phi)) * sinTheta + n * cosTheta;
}

float3 sampleCosineWeightedHemisphere(float3 n, float2 xi)
{
    float cosTheta = sqrt(xi.x);
    return alignToDirection(n, cosTheta, xi.y * DOUBLE_PI);
}

float3 sampleGGXDistribution(float3 n, float2 xi, float alpha)
{
    float cosTheta = sqrt((1.0f - xi.x) / (xi.x * (alpha * alpha - 1.0f) + 1.0f));
    return alignToDirection(n, cosTheta, xi.y * DOUBLE_PI);
}

float powerHeuristic(float fPdf, float gPdf)
{
    float f2 = fPdf * fPdf;
    float g2 = gPdf * gPdf;
    return saturate(f2 / (f2 + g2));
}

float ggxNormalDistribution(float alphaSquared, float cosTheta)
{
    float denom = (alphaSquared - 1.0f) * cosTheta * cosTheta + 1.0f;
    return alphaSquared / (PI * denom * denom);
}

float ggxVisibility(float alphaSquared, float cosTheta)
{
    float cosThetaSquared = cosTheta * cosTheta;
    float tanThetaSquared = (1.0f - cosThetaSquared) / cosThetaSquared;
    return 2.0f / (1.0f + sqrt(1.0f + alphaSquared * tanThetaSquared));
}

float ggxVisibilityTerm(float alphaSquared, float t0, float t1)
{
    return ggxVisibility(alphaSquared, t0) * ggxVisibility(alphaSquared, t1);
}

float fresnelDielectric(float cosTheta)
{
    constexpr const float ior = 1.5;
    constexpr const float r0 = (ior - 1.0) / (ior + 1.0);
    constexpr const float r0squared = r0 * r0;

    return r0squared + (1.0f - r0squared) * pow(1.0f - cosTheta, 5.0f);
}

float fresnelConductor(float cosTheta)
{
    return 1.0f; // TODO
}

float2 directionToEquirectangularCoordinates(float3 d)
{
    d = normalize(d);
    float u = 0.5 - atan2(d.x, d.z) / DOUBLE_PI;
    float v = 0.5 - asin(d.y) / PI;
    return float2(u, v);
}
