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
    packed_float3 diffuse = {};
    uint type = MATERIAL_DIFFUSE;
    packed_float3 specular = {};
    float roughness = 0.0f;
    packed_float3 transmittance = {};
    float extIOR = 1.0f;
    packed_float3 emissive = {};
    float intIOR = 1.5f;
};

struct Triangle
{
    uint materialIndex = 0;
    float area = 0.0f;
    float discretePdf = 0.0f;
};

struct EmitterTriangle
{
    float area = 0.0f;
    float discretePdf = 0.0f;
    float scaledArea = 0.0f;
    float cdf = 0.0f;
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
    uint completed = 0;
    float misPdf = 0.0f;
    uint generation = 0;
    uint sampledLightTriangle = uint(-1);
};

struct LightSamplingRay
{
    MPSRayOriginMinDistanceDirectionMaxDistance base = {};
    uint targetPrimitiveIndex = 0;
    packed_float3 throughput = {};
};

struct ApplicationData
{
    packed_float3 environmentColor = {};
    float time = 0.0f;
    uint frameIndex = 0;
    uint emitterTrianglesCount = 0;
};

struct SampledMaterial
{
    packed_float3 direction = {};
    float pdf = 0.0f;
    packed_float3 bsdf = 0.0f;
    uint valid = 0;
    packed_float3 weight = 0.0f;
};

struct RandomSample
{
    packed_float2 barycentricSample = { };
    packed_float2 bsdfSample = { };
    packed_float2 emitterBsdfSample = { };
    float componentSample = 0.0f;
    float emitterSample = 0.0f;
    float rrSample = 0.0f;
};

struct LightSample
{
    packed_float3 value = { };
    float samplePdf = 0.0f;
    packed_float3 direction = { };
    float emitterPdf = 0.0f;
    uint primitiveIndex = 0;
    uint valid = 0;
};

struct FresnelSample
{
    float value = 0.0f;
    float etaI = 0.0f;
    float etaO = 0.0f;
};

using Intersection = MPSIntersectionDistancePrimitiveIndexCoordinates;
using LightSamplingIntersection = MPSIntersectionDistancePrimitiveIndex;
