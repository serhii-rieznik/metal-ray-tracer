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

using namespace metal;

kernel void generateRays(device Ray* rays [[buffer(0)]],
                         device vector_float4* noise [[buffer(1)]],
                         uint2 coordinates [[thread_position_in_grid]],
                         uint2 size [[threads_per_grid]])
{
    const float3 origin = float3(0.0f, 1.0f, 2.1f);

    uint noiseSampleIndex = (coordinates.x % NOISE_BLOCK_SIZE) + NOISE_BLOCK_SIZE * (coordinates.y % NOISE_BLOCK_SIZE);
    device const float4& noiseSample = noise[noiseSampleIndex];

    float aspect = float(size.x) / float(size.y);
    float2 uv = float2(coordinates) / float2(size - 1) * 2.0f - 1.0f;
    float2 rnd = (noiseSample.xy * 2.0 - 1.0) / float2(size - 1);
    float3 direction = normalize(float3(aspect * (uv.x + rnd.x), uv.y + rnd.y, -1.0f));

    uint rayIndex = coordinates.x + coordinates.y * size.x;
    rays[rayIndex].base.origin = origin;
    rays[rayIndex].base.direction = direction;
    rays[rayIndex].base.minDistance = DISTANCE_EPSILON;
    rays[rayIndex].base.maxDistance = INFINITY;
    rays[rayIndex].radiance = 0.0f;
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
    if (i.distance < DISTANCE_EPSILON)
    {
        lightSamplingRays[rayIndex].base.maxDistance = -1.0;
        lightSamplingRays[rayIndex].targetPrimitiveIndex = uint(-1);
        return;
    }

    uint noiseSampleIndex = (coordinates.x % NOISE_BLOCK_SIZE) + NOISE_BLOCK_SIZE * (coordinates.y % NOISE_BLOCK_SIZE);
    device const float4& noiseSample = noise[noiseSampleIndex];

    device const Triangle& triangle = triangles[i.primitiveIndex];
    device const Material& material = materials[triangle.materialIndex];
    rays[rayIndex].radiance += material.emissive;

    device const packed_uint3& triangleIndices = indices[i.primitiveIndex];
    device const Vertex& a = vertices[triangleIndices.x];
    device const Vertex& b = vertices[triangleIndices.y];
    device const Vertex& c = vertices[triangleIndices.z];
    Vertex currentVertex = interpolate(a, b, c, i.coordinates);

    // sample light
    {
        device const EmitterTriangle& emitterTriangle =
            sampleEmitterTriangle(emitterTriangles, appData.emitterTrianglesCount, noiseSample.x);

        float3 lightTriangleBarycentric = barycentric(noiseSample.yz);
        Vertex lightVertex = interpolate(emitterTriangle.v0, emitterTriangle.v1, emitterTriangle.v2, lightTriangleBarycentric);
        packed_float3 directionToLight = normalize(lightVertex.v - currentVertex.v);

        lightSamplingRays[rayIndex].base.origin = currentVertex.v + currentVertex.n * DISTANCE_EPSILON;
        lightSamplingRays[rayIndex].base.direction = directionToLight;
        lightSamplingRays[rayIndex].base.minDistance = 0.0f;
        lightSamplingRays[rayIndex].base.maxDistance = INFINITY;
        lightSamplingRays[rayIndex].targetPrimitiveIndex = emitterTriangle.globalIndex;
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
        rays[rayIndex].radiance += 1.0f;
    }
}
