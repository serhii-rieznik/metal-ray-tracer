//
//  structures.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include <MetalPerformanceShaders/MetalPerformanceShaders.h>
#include "constants.h"

#if !defined(__METAL_VERSION__)
using packed_float3 = MPSPackedFloat3;
using uint = uint32_t;
#endif

struct Vertex
{
    packed_float3 v = {};
    packed_float3 n = {};
    packed_float2 t = {};
};

struct Material
{
    packed_float3 color = {};
    uint type = MATERIAL_DIFFUSE;
    packed_float3 emissive = {};
    float roughness = 0.0f;
};

struct Triangle
{
    float emitterPdf = 0.0f;
    float area = 0.0f;
    uint materialIndex = 0;
};

struct EmitterTriangle
{
    float area = 0.0f;
    float cdf = 0.0f;
    float pdf = 0.0f;
    uint globalIndex = 0;
    Vertex v0;
    Vertex v1;
    Vertex v2;
    packed_float3 emissive = {};
};

struct Ray
{
    MPSRayOriginMinDistanceDirectionMaxDistance base = {};
    packed_float3 radiance = {};
    uint bounces = 0;
    packed_float3 throughput = {};
    float lightSamplePdf = 0.0f;
    float materialSamplePdf = 0.0f;
};

struct LightSamplingRay
{
    MPSRayOriginMinDistanceDirectionMaxDistance base = {};
    uint targetPrimitiveIndex = 0;
    packed_float3 throughput = {};
};

struct ApplicationData
{
    uint frameIndex = 0;
    uint emitterTrianglesCount = 0;
};

struct SampledMaterial
{
    packed_float3 direction = {};
    float bsdf = 0.0f;
    float pdf = 0.0f;
    float bsdf_over_pdf = 0.0f;
};

using Intersection = MPSIntersectionDistancePrimitiveIndexCoordinates;
using LightSamplingIntersection = MPSIntersectionDistancePrimitiveIndex;
