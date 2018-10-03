//
//  base-ray-tracing.metal
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#include <metal_stdlib>
#include "structures.h"
#include "raytracing.h"
#include "materials.h"

using namespace metal;

kernel void generateRays(device Ray* rays [[buffer(0)]],
                         device vector_float4* noise [[buffer(1)]],
                         uint2 coordinates [[thread_position_in_grid]],
                         uint2 size [[threads_per_grid]])
{
    const float3 origin = float3(0.0f, 2.0f, 15.0f);

    uint noiseSampleIndex = (coordinates.x % NOISE_BLOCK_SIZE) + NOISE_BLOCK_SIZE * (coordinates.y % NOISE_BLOCK_SIZE);
    device const float4& noiseSample = noise[noiseSampleIndex];

    float aspect = float(size.y) / float(size.x);
    float2 uv = float2(coordinates) / float2(size - 1) * 2.0f - 1.0f;
    float2 rnd = (noiseSample.xy * 2.0 - 1.0) / float2(size - 1);
    float3 direction = normalize(float3((uv.x + rnd.x) * 0.4, aspect * (uv.y + rnd.y) * 0.4 - 0.3, -1.0f));

    uint rayIndex = coordinates.x + coordinates.y * size.x;
    rays[rayIndex].base.origin = origin;
    rays[rayIndex].base.direction = direction;
    rays[rayIndex].base.minDistance = DISTANCE_EPSILON;
    rays[rayIndex].base.maxDistance = INFINITY;
    rays[rayIndex].radiance = 0.0f;
    rays[rayIndex].throughput = 1.0f;
    rays[rayIndex].bounces = 0;
}

kernel void handleIntersections(device const Intersection* intersections [[buffer(0)]],
                                device const Material* materials [[buffer(1)]],
                                device const Triangle* triangles [[buffer(2)]],
                                device const EmitterTriangle* emitterTriangles [[buffer(3)]],
                                device const Vertex* vertices [[buffer(4)]],
                                device packed_uint3* indices [[buffer(5)]],
                                device const float4* noise [[buffer(6)]],
                                device Ray* rays [[buffer(7)]],
                                device LightSamplingRay* lightSamplingRays [[buffer(8)]],
                                constant ApplicationData& appData [[buffer(9)]],
                                uint2 coordinates [[thread_position_in_grid]],
                                uint2 size [[threads_per_grid]])
{
    uint rayIndex = coordinates.x + coordinates.y * size.x;
    device const Intersection& i = intersections[rayIndex];
    device Ray& currentRay = rays[rayIndex];
    device LightSamplingRay& lightSamplingRay = lightSamplingRays[rayIndex];

    if (i.distance < DISTANCE_EPSILON)
    {
        currentRay.base.maxDistance = -1.0;
        lightSamplingRay.base.maxDistance = -1.0;
        lightSamplingRay.targetPrimitiveIndex = uint(-1);
        return;
    }

    uint noiseSampleIndex = (coordinates.x % NOISE_BLOCK_SIZE) + (coordinates.y % NOISE_BLOCK_SIZE) * NOISE_BLOCK_SIZE +
        currentRay.bounces * NOISE_BLOCK_SIZE * NOISE_BLOCK_SIZE;

    device const float4& noiseSample = noise[noiseSampleIndex];
    device const Triangle& triangle = triangles[i.primitiveIndex];
    device const Material& material = materials[triangle.materialIndex];
    device const packed_uint3& triangleIndices = indices[i.primitiveIndex];
    device const Vertex& a = vertices[triangleIndices.x];
    device const Vertex& b = vertices[triangleIndices.y];
    device const Vertex& c = vertices[triangleIndices.z];
    
    Vertex currentVertex = interpolate(a, b, c, i.coordinates);

    {
    #if (IS_MODE == IS_MODE_MIS)

        packed_float3 directionToLight = currentVertex.v - currentRay.base.origin;
        float distanceToLight = length(directionToLight);
        directionToLight /= distanceToLight;
        float cosTheta = -dot(directionToLight, currentVertex.n);
        float lightSamplePdf = currentRay.lightSamplePdf * triangle.emitterPdf *
            (distanceToLight * distanceToLight) / (triangle.area * cosTheta);
        float weight = powerHeuristic(currentRay.materialSamplePdf, lightSamplePdf);

    #elif (IS_MODE == IS_MODE_BSDF)

        float weight = 1.0f;

    #else

        float weight = float(currentRay.bounces == 0);

    #endif

        float3 emissiveScale = currentRay.throughput * weight;
        currentRay.radiance += material.emissive * ((currentRay.bounces == 0) ? 1.0f : emissiveScale);
    }

    // sample light
    {
        device const EmitterTriangle& emitterTriangle =
            sampleEmitterTriangle(emitterTriangles, appData.emitterTrianglesCount, noiseSample.x);

        float3 lightTriangleBarycentric = barycentric(noiseSample.yz);
        Vertex lightVertex = interpolate(emitterTriangle.v0, emitterTriangle.v1, emitterTriangle.v2, lightTriangleBarycentric);

        packed_float3 directionToLight = lightVertex.v - currentVertex.v;
        float distanceToLight = length(directionToLight);
        directionToLight /= distanceToLight;

        float cosTheta = -dot(directionToLight, lightVertex.n);
        float lightSamplePdf = emitterTriangle.pdf * (distanceToLight * distanceToLight) / (emitterTriangle.area * cosTheta);

        SampledMaterial materialSample = sampleMaterial(material, currentVertex.n, currentRay.base.direction, directionToLight);

    #if (IS_MODE == IS_MODE_MIS)
        float weight = powerHeuristic(lightSamplePdf, materialSample.pdf);
    #elif (IS_MODE == IS_MODE_LIGHT)
        float weight = 1.0f;
    #else
        float weight = 0.0f;
    #endif

        bool validRay = (cosTheta > 0.0f) && (materialSample.bsdf > 0.0);

        lightSamplingRay.base.origin = currentVertex.v + currentVertex.n * DISTANCE_EPSILON;
        lightSamplingRay.base.direction = directionToLight;
        lightSamplingRay.base.minDistance = 0.0f;
        lightSamplingRay.base.maxDistance = validRay ? INFINITY : -1.0;
        lightSamplingRay.targetPrimitiveIndex = emitterTriangle.globalIndex;
        lightSamplingRay.throughput = emitterTriangle.emissive * currentRay.throughput * material.color *
            (materialSample.bsdf / lightSamplePdf * weight);
    }

    // generate next bounce
    {
        SampledMaterial materialSample = sampleMaterial(material, currentVertex.n, currentRay.base.direction, noiseSample);
        currentRay.base.origin = currentVertex.v + currentVertex.n * DISTANCE_EPSILON;
        currentRay.base.direction = materialSample.direction;
        currentRay.materialSamplePdf = materialSample.pdf;
        currentRay.lightSamplePdf = float(material.type != MATERIAL_MIRROR);
        currentRay.throughput *= material.color * materialSample.bsdf_over_pdf;
        currentRay.bounces += 1;
    }
}

kernel void lightSamplingHandler(device const LightSamplingIntersection* intersections [[buffer(0)]],
                                 device const LightSamplingRay* lightSamplingRays [[buffer(1)]],
                                 device Ray* rays [[buffer(2)]],
                                 uint2 coordinates [[thread_position_in_grid]],
                                 uint2 size [[threads_per_grid]])
{
    uint rayIndex = coordinates.x + coordinates.y * size.x;
    if (intersections[rayIndex].primitiveIndex == lightSamplingRays[rayIndex].targetPrimitiveIndex)
    {
        rays[rayIndex].radiance += lightSamplingRays[rayIndex].throughput;
    }
}
