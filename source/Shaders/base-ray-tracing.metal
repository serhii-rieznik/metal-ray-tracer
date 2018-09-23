//
//  base-ray-tracing.metal
//  Metal ray-tracer
//
//  Created by Sergey Reznik on 9/15/18.
//  Copyright © 2018 Serhii Rieznik. All rights reserved.
//

#include <metal_stdlib>
#include "structures.h"

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
                                device Ray* rays [[buffer(3)]],
                                uint2 coordinates [[thread_position_in_grid]],
                                uint2 size [[threads_per_grid]])
{
    uint rayIndex = coordinates.x + coordinates.y * size.x;
    device const Intersection& i = intersections[rayIndex];
    if (i.distance < DISTANCE_EPSILON)
        return;

    device const Triangle& triangle = triangles[i.primitiveIndex];
    device const Material& material = materials[triangle.materialIndex];
    rays[rayIndex].radiance += material.diffuse;
}
