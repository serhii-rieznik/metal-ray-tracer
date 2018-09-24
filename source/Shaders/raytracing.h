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
    result.n = a.n * u + b.n * v + c.n * w;
    result.t = a.t * u + b.t * v + c.t * w;
    return result;
}

Vertex interpolate(device const Vertex& a, device const Vertex& b, device const Vertex& c, float2 barycentric)
{
    float u = barycentric.x;
    float v = barycentric.y;
    float w = 1.0f - u - v;

    Vertex result;
    result.v = a.v * u + b.v * v + c.v * w;
    result.n = a.n * u + b.n * v + c.n * w;
    result.t = a.t * u + b.t * v + c.t * w;
    return result;
}

device const EmitterTriangle& sampleEmitterTriangle(device const EmitterTriangle* triangles, uint triangleCount, float xi)
{
    uint index = 0;
    for (; (index < triangleCount) && (triangles[index + 1].cdf <= xi); ++index);
    return triangles[index];
}

float3 barycentric(float2 smp)
{
    float r1 = sqrt(smp.x);
    float r2 = smp.y;
    return float3(1.0f - r1, r1 * (1.0f - r2), r1 * r2);
}
