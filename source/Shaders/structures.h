//
//  structures.h
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#pragma once

#include <MetalPerformanceShaders/MetalPerformanceShaders.h>
#include "gpu-spectrum.h"

struct Vertex
{
    packed_float3 v = {};
    packed_float3 n = {};
    packed_float2 t = {};
};

struct Material
{
    GPUSpectrum diffuse;
    GPUSpectrum specular;
    GPUSpectrum transmittance;
    GPUSpectrum emissive;
    GPUSpectrum extIOR;
    GPUSpectrum intIOR_eta;
    GPUSpectrum intIOR_k;
    uint type = MATERIAL_DIFFUSE;
    float roughness = 1.0f;
    float coatingThickness = 0.0f;
    float coatingIOR = 1.5f;
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
    GPUSpectrum emissive = {};
};

struct Ray
{
    MPSRayOriginMinDistanceDirectionMaxDistance base = {};
    float wavelength = 0.0f;
    float radiance = 0.0f;
    float throughput = 0.0f;
    float misPdf = 0.0f;
    uint bounces = 0;
    uint completed = 0;
    uint generation = 0;
};

struct LightSamplingRay
{
    MPSRayOriginMinDistanceDirectionMaxDistance base = {};
    float throughput = {};
    packed_float3 n = {};
    uint targetPrimitiveIndex = 0;
};

struct Camera
{
    packed_float3 origin;
    float fov = 90.0f;
    packed_float3 target;
    packed_float3 up;
};

struct ApplicationData
{
    GPUSpectrum environmentColor;
    float time = 0.0f;
    uint frameIndex = 0;
    uint emitterTrianglesCount = 0;
    uint comparisonMode = COMPARE_DISABLED;
    Camera camera;
};

struct SampledMaterial
{
    packed_float3 normal = {};
    packed_float3 direction = {};
    uint valid = 0;
    float bsdf;
    float weight;
    float pdf = 0.0f;
};

struct RandomSample
{
    packed_float2 pixelSample = { };
    packed_float2 barycentricSample = { };
    packed_float2 bsdfSample = { };
    packed_float2 emitterBsdfSample = { };
    float wavelengthSample = 0.0f;
    float componentSample = 0.0f;
    float emitterSample = 0.0f;
    float rrSample = 0.0f;
};

struct LightSample
{
    packed_float3 direction = { };
    uint valid = 0;
    float value = 0.0f;
    float samplePdf = 0.0f;
    float emitterPdf = 0.0f;
    uint primitiveIndex = 0;
};

struct LightPositionSample
{
    packed_float3 origin = { };
    packed_float3 direction = { };
    float value = 0.0f;
};

using Intersection = MPSIntersectionDistancePrimitiveIndexCoordinates;
using LightSamplingIntersection = MPSIntersectionDistancePrimitiveIndex;
