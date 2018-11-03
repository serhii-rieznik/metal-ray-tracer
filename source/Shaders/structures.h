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
    float emitterPdf = 0.0f;
    float area = 0.0f;
    uint materialIndex = 0;
};

struct EmitterTriangle
{
    float area = 0.0f;
    float scaledArea = 0.0f;
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
    float misPdf = 0.0f;
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

struct SampledMaterialProperties
{
    packed_float3 bsdfDiffuse = 0.0f;
    float pdfDiffuse = 0.0f;
    packed_float3 bsdfSpecular = 0.0f;
    float pdfSpecular = 0.0f;
    packed_float3 bsdfTransmittance = 0.0f;
    float pdfTransmittance = 0.0f;
};

struct SampledMaterial
{
    packed_float3 direction = {};
    float misPdf = 0.0f;
    packed_float3 weight;
};

using Intersection = MPSIntersectionDistancePrimitiveIndexCoordinates;
using LightSamplingIntersection = MPSIntersectionDistancePrimitiveIndex;
